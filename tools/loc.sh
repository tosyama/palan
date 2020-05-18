#!/bin/bash

find src -type d -name test -prune \
	-o -type d -name ast -prune \
	-o -type f -name '*.cpp' -print \
	-o -type f -name '*.h' -print \
| xargs wc -l

echo ""

find src/ast \
	 -type f -name '*.cpp' \
	 -not -name 'PlnParser.cpp' -not -name 'PlnLexer.cpp' -print \
	 -o -type f -name '*.h' -print \
	 -o -type f -name '*.yy' -print \
	 -o -type f -name '*.ll' -print \
 | xargs wc -l

