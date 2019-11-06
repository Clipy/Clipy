#!/usr/bin/ruby -w

require 'rexml/document'
include REXML

def process_doc(doc)
	rss = doc.root
	rss.elements.each('channel') {
		|channel| process_channel(channel)
	}
end

def process_channel(channel)
	channel_name = channel.get_text('title').value
	STDERR.puts 'Channel: %s' % [channel_name]
	channel.elements.each('item') {
		|item| process_item(item, channel_name)
	}
end

def process_item(item, channel_name) 
	version = item.get_text('title')
	version_name = version.value
	STDERR.puts '  %s: %s' % [channel_name, version_name]
	item.elements.each('sparkle:releaseNotesLink') {
		|link| item.delete_element(link)
	}
	link = Element.new('sparkle:releaseNotesLink')
	link.text = 'https://github.com/ian4hu/%s/blob/%s/CHANGELOG.md' % [channel_name ,version_name]
	version.parent.next_sibling = link
end

doc = Document.new(STDIN)
process_doc(doc)
doc.write(STDOUT, 4)