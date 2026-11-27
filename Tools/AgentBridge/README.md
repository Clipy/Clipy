# Clipy agent bridge (experimental)

Connect an MCP-compatible agent to **saved text history** using a local stdio
process. This is an optional companion script, not a background web server.
It requires Python 3.9+ and a Clipy build containing the SQLite history store
and the **Settings > Beta > Allow agents to query text history** toggle.
Older Realm-based releases are not supported. The bridge is not automatically
installed or started by Clipy.

## Connect

1. Build and launch Clipy following the repository README. Debug builds have a
   separate database and preference domain from release builds.
2. Read the privacy warning in Settings > Beta and enable agent history access
   only if you want the connected agent to read your saved text.
3. Keep `clipy_mcp.py` in a stable local directory. Find your Python 3 executable
   with `command -v python3`. Configure your MCP client to launch it with the
   script's absolute path. For clients using `mcpServers` JSON:

   ```json
   {
     "mcpServers": {
       "clipy": {
         "command": "/absolute/path/to/python3",
         "args": ["/absolute/path/to/clipy_mcp.py"]
       }
     }
   }
   ```

   Append `"--debug"` to `args` for an Xcode Debug build. Do not enable both
   history stores unless you intend to share both. Client configuration formats
   vary; select **local / stdio**, not a remote URL. There are no API keys.
4. Restart/reconnect your client's MCP session. Ask it to call
   `search_clipboard_history` with `{"query":"meeting","limit":5}`.
   An empty query returns the newest text entries. The maximum limit is 20.

Turn the setting off to reject subsequent queries, including in already-running
bridge processes. Remove the MCP client configuration to stop launching it.
Closing Clipy does not itself revoke access; the saved database remains on disk.

## Scope and privacy

- Disabled by default. The script reads the same build's preference through
  macOS `defaults` before and after each query. It fails closed if that check fails.
- Reads SQLite with `mode=ro` and `query_only`, with bounded query time and no
  persistent connection, clipboard writes, deletion, shell tool, telemetry,
  HTTP listener, credentials, or network code.
- Only entries containing the `public.utf8-plain-text` or legacy `NSStringPboardType` type are
  searched. Images, OCR-only entries, file-only entries and snippets are not
  returned. Text entries may still contain file paths or secrets copied as text.
- Search is a literal substring of the **saved preview**, not the full clipboard
  payload. Clipy limits new saved previews to 10,000 characters; each returned
  text is capped at 4,000 characters with a `truncated` flag. Search can match
  beyond the returned portion. SQLite's built-in `lower()` case-folds ASCII,
  not all Unicode languages. Results are newest first, with ID as a stable tie-break.
- `createdAt` and `updatedAt` are Unix seconds. Pasting an entry may change its
  update timestamp according to Clipy's settings.
- SQLite WAL changes and history deletions become visible on the next query.
  No exported copy or separate search index is created by the bridge.
- A connected agent can send returned content to its model provider and retain
  it. Disabling access or deleting history cannot recall data already shared.
  Exclude sensitive apps and review the client's tool approvals/privacy settings.
- **This toggle is a cooperative privacy control, not a security boundary against
  other programs running as your macOS user.** Such programs may already read
  Clipy's unencrypted database, change preferences, or run a modified bridge.
- Clipboard text is untrusted input, including possible prompt injection. The
  tool warns clients not to interpret returned text as instructions. A client
  remains responsible for enforcing that boundary and asking before any action.

## Development

No third-party Python dependencies are needed. From the repository root:

```sh
python3 -m unittest discover -s Tools/AgentBridge -p 'test_*.py' -v
```

The bridge implements newline-delimited JSON-RPC over stdio and MCP protocol
`2024-11-05`: `initialize`, `notifications/initialized`, `ping`, `tools/list`,
and `tools/call`. Only stdout carries protocol messages. Malformed and oversized
requests receive errors without logging their contents. Newer clients must
accept this negotiated protocol version or decline the connection.

Tests use temporary synthetic SQLite databases; never use real clipboard data
in test fixtures, logs, issues, or pull requests. Full acceptance also requires
building Clipy on supported macOS/Xcode, testing the toggle with `defaults`, and
connecting a real MCP client against synthetic history. Python tests alone do
not validate that macOS integration.
