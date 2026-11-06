"""Synthetic-data bridge tests; run: python3 -m unittest discover -s Tools/AgentBridge -v."""
import io
import json
from pathlib import Path
import re
import sqlite3
import subprocess
import tempfile
import unittest
import uuid
from unittest import mock

import clipy_mcp as mcp


class BridgeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        # Exercise URI escaping as well as a realistic application schema.
        self.database = Path(self.temp.name) / 'history # café?.db'
        self.writer = sqlite3.connect(self.database)
        self.writer.create_function("uuid", 0, lambda: str(uuid.uuid4()))
        self.addCleanup(self.writer.close)
        migrations = Path(__file__).resolve().parents[2] / 'Clipy/Sources/Database/Migrations'
        # Execute the actual migration SQL, including STRICT tables, FTS and triggers,
        # rather than maintaining a simplified schema which could hide regressions.
        for version in range(1, 5):
            source = (migrations / f'SQLiteDataMigrator+V{version}.swift').read_text()
            statements = re.findall(r'"""(.*?)"""', source, re.S)
            self.assertTrue(statements)
            for statement in statements:
                self.writer.execute(statement)
        self.writer.commit()
        self.enabled = mock.Mock(return_value=True)
        self.bridge = mcp.Bridge(self.database, self.enabled)

    def add(self, identifier, text, updated=10, types=None, ocr=None):
        if types is None:
            types = ['public.utf8-plain-text']
        self.writer.execute(
            'INSERT INTO pasteboardHistories '
            '(id,title,pasteboardTypes,createdAt,updateAt,ocrText) VALUES (?,?,?,?,?,?)',
            (identifier, text, json.dumps(types), 1, updated, ocr))
        self.writer.commit()

    def request(self, method, params=None, identifier=1):
        request = {'jsonrpc': '2.0', 'id': identifier, 'method': method}
        if params is not None:
            request['params'] = params
        return self.bridge.handle(request)

    def initialize(self):
        response = self.request('initialize', {'protocolVersion': mcp.PROTOCOL})
        self.assertIn('result', response)
        self.bridge.handle({'jsonrpc': '2.0', 'method': 'notifications/initialized'})

    def call(self, arguments=None):
        return self.request('tools/call', {'name': mcp.TOOL['name'], 'arguments': arguments or {}})

    def stream(self, data):
        output = io.StringIO()
        mcp.serve(self.bridge, io.BytesIO(data), output)
        return [json.loads(line) for line in output.getvalue().splitlines()]

    def test_initialize_requires_version_and_ready_notification(self):
        self.assertEqual(self.request('initialize')['error']['code'], -32602)
        self.assertFalse(self.bridge.initialized)
        self.assertEqual(self.request('tools/list')['error']['code'], -32000)
        initialized = self.request('initialize', {'protocolVersion': 'future-version'}, 'init')
        self.assertEqual(initialized['id'], 'init')
        self.assertEqual(initialized['result']['protocolVersion'], mcp.PROTOCOL)
        self.assertEqual(initialized['result']['capabilities'], {'tools': {}})
        self.assertIn('untrusted', initialized['result']['instructions'])
        self.assertEqual(self.request('tools/list')['error']['code'], -32000)
        self.bridge.handle({'jsonrpc': '2.0', 'method': 'notifications/initialized'})
        self.assertEqual(self.request('tools/list')['result']['tools'], [mcp.TOOL])
        self.assertEqual(self.request('initialize', {'protocolVersion': mcp.PROTOCOL})['error']['code'], -32600)

    def test_premature_ready_notification_does_not_initialize(self):
        self.bridge.handle({'jsonrpc': '2.0', 'method': 'notifications/initialized'})
        self.assertFalse(self.bridge.ready)

    def test_ping_before_and_after_initialization(self):
        self.assertEqual(self.request('ping')['result'], {})
        self.initialize()
        self.assertEqual(self.request('ping')['result'], {})

    def test_notifications_never_read_clipboard_or_reply(self):
        self.initialize()
        with mock.patch.object(self.bridge, 'search') as search:
            for method in ['tools/call', 'tools/list', 'initialize', 'unknown']:
                self.assertIsNone(self.bridge.handle({'jsonrpc': '2.0', 'method': method,
                                                     'params': {'name': mcp.TOOL['name']}}))
            search.assert_not_called()

    def test_invalid_requests(self):
        for request in [None, [], 1, 'hello', {}, {'jsonrpc': '1.0', 'method': 'ping'},
                        {'jsonrpc': '2.0', 'method': 3},
                        *[{'jsonrpc': '2.0', 'method': 'ping', 'id': value}
                          for value in [None, True, 1.5, [], {}]]]:
            with self.subTest(request=request):
                self.assertEqual(self.bridge.handle(request)['error']['code'], -32600)

    def test_invalid_params_and_unknown_methods(self):
        for value in [[], 'invalid', 3, True]:
            self.assertEqual(self.request('ping', value)['error']['code'], -32602)
            self.assertIsNone(self.bridge.handle({'jsonrpc': '2.0', 'method': 'ping', 'params': value}))
        self.initialize()
        self.assertEqual(self.request('unknown')['error']['code'], -32601)
        self.assertEqual(self.request('tools/call', {'name': 'delete_history'})['error']['code'], -32602)

    def test_ndjson_multiple_requests_and_notifications(self):
        frames = [dict(jsonrpc='2.0', id=1, method='initialize', params={'protocolVersion': mcp.PROTOCOL}),
                  dict(jsonrpc='2.0', method='notifications/initialized'),
                  dict(jsonrpc='2.0', id='two', method='tools/list')]
        responses = self.stream(('\r\n'.join(map(json.dumps, frames)) + '\n').encode())
        self.assertEqual([response['id'] for response in responses], [1, 'two'])
        self.assertIn('tools', responses[1]['result'])

    def test_bad_json_and_utf8_recover_for_next_frame(self):
        responses = self.stream(b'{invalid}\n\xff\n\n' + b'{"jsonrpc":"2.0","id":4,"method":"ping"}\n')
        self.assertEqual([response['error']['code'] for response in responses[:3]], [-32700] * 3)
        self.assertEqual(responses[-1]['result'], {})

    def test_oversize_frame_is_drained_and_next_request_survives(self):
        responses = self.stream(b'x' * (3 * mcp.MAX_LINE) + b'\n' +
                                b'{"jsonrpc":"2.0","id":4,"method":"ping"}\n')
        self.assertEqual(len(responses), 2)
        self.assertEqual(responses[0]['error']['code'], -32600)
        self.assertEqual(responses[1]['id'], 4)

    def test_frame_boundary_eof_and_empty_stream(self):
        ping = b'{"jsonrpc":"2.0","id":4,"method":"ping"}'
        self.assertEqual(self.stream(b''), [])
        self.assertEqual(self.stream(ping)[0]['result'], {})
        self.assertEqual(self.stream(ping + b' ' * (mcp.MAX_LINE - len(ping) - 1) + b'\n')[0]['result'], {})
        self.assertEqual(self.stream(b'x' * (mcp.MAX_LINE + 1))[0]['error']['code'], -32600)

    def test_deep_json_does_not_kill_stream(self):
        responses = self.stream(b'[' * 30000 + b'0' + b']' * 30000 + b'\n' +
                                b'{"jsonrpc":"2.0","id":2,"method":"ping"}\n')
        # Python < 3.14 raises RecursionError (parse error); 3.14+ parses it as a non-object request.
        self.assertIn(responses[0]['error']['code'], (-32700, -32600))
        self.assertEqual(responses[1]['result'], {})

    def test_consent_denied_before_database_access(self):
        self.enabled.return_value = False
        with mock.patch.object(mcp.sqlite3, 'connect') as connect:
            with self.assertRaises(PermissionError):
                self.bridge.search({})
            connect.assert_not_called()

    def test_consent_revocation_during_and_between_queries(self):
        self.add('one', 'synthetic secret')
        self.enabled.side_effect = [True, False]
        with self.assertRaises(PermissionError):
            self.bridge.search({})
        self.enabled.side_effect = None
        self.enabled.return_value = True
        self.assertEqual(len(self.bridge.search({})['items']), 1)
        self.enabled.return_value = False
        with self.assertRaises(PermissionError):
            self.bridge.search({})

    def test_missing_database_never_created(self):
        missing = Path(self.temp.name) / 'never-created.db'
        self.bridge.database = missing
        with self.assertRaises(sqlite3.Error):
            self.bridge.search({})
        self.assertFalse(missing.exists())

    def test_connection_is_read_only_and_closed(self):
        self.add('one', 'synthetic')
        original_connect = sqlite3.connect
        connections = []
        def inspect_connection(database_uri, **kwargs):
            self.assertTrue(database_uri.endswith('?mode=ro'))
            self.assertTrue(kwargs['uri'])
            connection = original_connect(database_uri, **kwargs)
            with self.assertRaises(sqlite3.OperationalError):
                connection.execute("DELETE FROM pasteboardHistories")
            connections.append(connection)
            return connection
        before = self.database.read_bytes()
        with mock.patch.object(mcp.sqlite3, 'connect', side_effect=inspect_connection):
            self.bridge.search({})
        self.assertEqual(before, self.database.read_bytes())
        with self.assertRaises(sqlite3.ProgrammingError):
            connections[0].execute('SELECT 1')

    def test_literal_wildcards_quotes_and_sql_injection(self):
        samples = ['100% done', 'under_score', "quote ' OR 1=1 --", 'square[bracket]', 'back\\slash', 'ordinary']
        for i, sample in enumerate(samples):
            self.add(str(i), sample)
        for query, identifier in [('%', '0'), ('_', '1'), ("' OR 1=1 --", '2'), ('[', '3'), ('\\', '4')]:
            with self.subTest(query=query):
                self.assertEqual([item['id'] for item in self.bridge.search({'query': query})['items']], [identifier])
        self.assertEqual(self.bridge.search({'query': "'; DROP TABLE pasteboardHistories; --"})['items'], [])
        self.assertEqual(self.writer.execute('SELECT count(*) FROM pasteboardHistories').fetchone()[0], 6)

    def test_limits_defaults_and_argument_types(self):
        for i in range(25):
            self.add(f'{i:02}', 'test', updated=i)
        self.assertEqual(len(self.bridge.search({})['items']), 5)
        for limit in [1, 20]:
            self.assertEqual(len(self.bridge.search({'limit': limit})['items']), limit)
        for arguments in [None, [], '', {'extra': 1}, *[{'limit': x} for x in [0, -1, 21, True, False, 1.0, '1', None]],
                          *[{'query': x} for x in [None, False, 1, [], {}, 'x' * 257]]]:
            with self.subTest(arguments=arguments), self.assertRaises(ValueError):
                self.bridge.search(arguments)
        self.assertEqual(self.bridge.search({'query': 'x' * 256})['items'], [])

    def test_lone_surrogate_query_is_a_tool_validation_error(self):
        self.initialize()
        for query in ['\ud800', '\udfff', 'prefix\ud800suffix']:
            with self.subTest(query=repr(query)):
                with self.assertRaises(ValueError):
                    self.bridge.search({'query': query})
                request = {'jsonrpc': '2.0', 'id': 'surrogate', 'method': 'tools/call',
                           'params': {'name': mcp.TOOL['name'], 'arguments': {'query': query}}}
                responses = self.stream((json.dumps(request) + '\n').encode())
                self.assertEqual(len(responses), 1)
                self.assertEqual(responses[0]['id'], 'surrogate')
                self.assertNotIn('error', responses[0])
                self.assertTrue(responses[0]['result']['isError'])
                self.assertNotIn(query, responses[0]['result']['content'][0]['text'])
        self.enabled.assert_not_called()

    def test_order_timestamps_and_unicode(self):
        self.add('b', 'Café 東京 🐈\nSynthetic', 30)
        self.add('a', 'other', 30)
        self.add('old', 'old', 2)
        items = self.bridge.search({})['items']
        self.assertEqual([item['id'] for item in items], ['a', 'b', 'old'])
        self.assertEqual(items[1]['createdAt'], 1)
        self.assertEqual(items[1]['updatedAt'], 30)
        for query in ['東京', '🐈', 'Café', 'synthetic']:
            self.assertEqual(self.bridge.search({'query': query})['items'][0]['id'], 'b')
        self.initialize()
        payload = json.loads(self.call({'query': '🐈'})['result']['content'][0]['text'])
        self.assertEqual(payload['items'][0]['text'], 'Café 東京 🐈\nSynthetic')

    def test_excludes_nontext_ocr_assets_and_snippets(self):
        self.add('image', 'image marker', types=['public.png'], ocr='hidden OCR')
        self.add('file', 'file marker', types=['public.file-url'])
        self.add('rtf', 'rich marker', types=['public.rtf'])
        self.add('empty', 'empty marker', types=[])
        self.add('text', 'visible preview', types=['public.rtf', 'public.utf8-plain-text'], ocr='hidden OCR')
        self.writer.execute("INSERT INTO snippetFolders (id,title) VALUES ('folder','test')")
        self.writer.execute("INSERT INTO snippets (id,folderID,title,content) VALUES ('snippet','folder','hidden snippet','secret content')")
        self.writer.execute("INSERT INTO pasteboardHistoryAssets (id,pasteboardHistoryID,pasteboardType,data) VALUES ('asset','text','public.utf8-plain-text',?)", (b'hidden full asset',))
        self.writer.commit()
        self.assertEqual([item['id'] for item in self.bridge.search({})['items']], ['text'])
        for query in ['hidden', 'secret content', 'image marker']:
            self.assertEqual(self.bridge.search({'query': query})['items'], [])

    def test_legacy_string_pasteboard_type_is_searchable(self):
        self.add('legacy', 'legacy text', types=['NSStringPboardType'])
        self.assertEqual(self.bridge.search({'query': 'legacy'})['items'][0]['id'], 'legacy')

    def test_fresh_updates_deletions_and_clear_history(self):
        self.add('one', 'before')
        self.assertEqual(self.bridge.search({})['items'][0]['text'], 'before')
        self.writer.execute("UPDATE pasteboardHistories SET title='after',updateAt=50 WHERE id='one'")
        self.writer.commit()
        self.assertEqual(self.bridge.search({})['items'][0]['text'], 'after')
        self.add('two', 'new', 60)
        self.writer.execute("DELETE FROM pasteboardHistories WHERE id='one'")
        self.writer.commit()
        self.assertEqual([item['id'] for item in self.bridge.search({})['items']], ['two'])
        self.writer.execute('DELETE FROM pasteboardHistories')
        self.writer.commit()
        self.assertEqual(self.bridge.search({})['items'], [])

    def test_reads_uncheckpointed_wal_and_excludes_uncommitted_rows(self):
        self.assertEqual(self.writer.execute('PRAGMA journal_mode=WAL').fetchone()[0], 'wal')
        self.writer.execute('PRAGMA wal_autocheckpoint=0')
        self.add('wal', 'committed WAL')
        self.assertTrue(Path(str(self.database) + '-wal').exists())
        self.assertEqual(self.bridge.search({})['items'][0]['id'], 'wal')
        self.writer.execute("UPDATE pasteboardHistories SET title='uncommitted' WHERE id='wal'")
        self.assertEqual(self.bridge.search({})['items'][0]['text'], 'committed WAL')
        self.writer.commit()
        self.assertEqual(self.bridge.search({})['items'][0]['text'], 'uncommitted')
        self.writer.execute('DELETE FROM pasteboardHistories')
        self.writer.commit()
        self.assertEqual(self.bridge.search({})['items'], [])

    def test_truncation_counts_characters_and_searches_full_preview(self):
        for size in [mcp.MAX_TEXT - 1, mcp.MAX_TEXT, mcp.MAX_TEXT + 1]:
            self.add(str(size), '🐈' * size, updated=size)
        for item in self.bridge.search({})['items']:
            size = int(item['id'])
            self.assertEqual(item['text'], '🐈' * min(size, mcp.MAX_TEXT))
            self.assertEqual(item['truncated'], size > mcp.MAX_TEXT)
        self.add('tail', 'a' * 9000 + 'needle', updated=10000)
        item = self.bridge.search({'query': 'needle'})['items'][0]
        self.assertEqual(item['id'], 'tail')
        self.assertTrue(item['truncated'])
        self.assertNotIn('needle', item['text'])

    def test_tool_errors_are_sanitized_and_use_tool_error_result(self):
        self.initialize()
        for exception in [sqlite3.OperationalError('secret SQL/path/query'), OSError('/private/secret')]:
            with mock.patch.object(self.bridge, 'search', side_effect=exception):
                response = self.call()
            self.assertTrue(response['result']['isError'])
            self.assertNotIn('secret', json.dumps(response))
            self.assertNotIn('error', response)
        self.enabled.return_value = False
        self.assertTrue(self.call()['result']['isError'])
        self.enabled.return_value = True
        self.assertTrue(self.call({'limit': True})['result']['isError'])

    def test_corrupt_database_and_malformed_json_are_sanitized(self):
        self.initialize()
        self.add('bad', 'synthetic marker')
        self.writer.execute("UPDATE pasteboardHistories SET pasteboardTypes='not json secret'")
        self.writer.commit()
        response = self.call()
        self.assertTrue(response['result']['isError'])
        self.assertNotIn('secret', json.dumps(response))
        corrupt = Path(self.temp.name) / 'secret-corrupt.db'
        corrupt.write_bytes(b'not a SQLite database synthetic secret')
        self.bridge.database = corrupt
        self.assertTrue(self.call()['result']['isError'])
        self.assertNotIn(str(corrupt), json.dumps(self.call()))


class PreferenceTests(unittest.TestCase):
    def test_enabled_only_for_explicit_successful_one(self):
        for status, stdout, expected in [(0, '1\n', True), (0, '0', False), (1, '1', False),
                                         (0, '', False), (0, 'true', False), (0, '11', False)]:
            with self.subTest(status=status, stdout=stdout), mock.patch.object(mcp.subprocess, 'run') as run:
                run.return_value = subprocess.CompletedProcess([], status, stdout=stdout)
                self.assertEqual(mcp.is_enabled('com.clipy-app.Clipy.debug'), expected)
                self.assertEqual(run.call_args.args[0], ['/usr/bin/defaults', 'read', 'com.clipy-app.Clipy.debug', mcp.PREFERENCE])
                self.assertEqual(run.call_args.kwargs['timeout'], 2)

    def test_preferences_fail_closed(self):
        for failure in [OSError('unavailable'), subprocess.TimeoutExpired('defaults', 2)]:
            with mock.patch.object(mcp.subprocess, 'run', side_effect=failure):
                self.assertFalse(mcp.is_enabled('com.clipy-app.Clipy'))

    def test_cli_release_and_debug_paths_and_consent_domains(self):
        for debug in [False, True]:
            bundle = 'com.clipy-app.Clipy' + ('.debug' if debug else '')
            with mock.patch.object(mcp.sys, 'argv', ['clipy_mcp.py'] + (['--debug'] if debug else [])), \
                    mock.patch.object(mcp.Path, 'home', return_value=Path('/synthetic-home')), \
                    mock.patch.object(mcp, 'serve') as serve, mock.patch.object(mcp, 'is_enabled', return_value=False) as enabled:
                mcp.main()
                bridge = serve.call_args.args[0]
                self.assertEqual(bridge.database, Path('/synthetic-home/Library/Application Support') / bundle / 'sqlite.db')
                self.assertFalse(bridge.enabled())
                enabled.assert_called_once_with(bundle)


if __name__ == '__main__':
    unittest.main()
