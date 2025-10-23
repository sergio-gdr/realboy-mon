sources = files(
	'main.c',
	'client.c',
	'disasm.c',
	'tui/cli.c',
	'tui/tui.c',
)

executable(
	'monitor',
	sources,
	dependencies: [dependency('ncurses'), dependency('libemu')],
	install: true
)
