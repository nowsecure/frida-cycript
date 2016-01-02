# Cycript - The Truly Universal Scripting Language
# Copyright (C) 2009-2016  Jay Freeman (saurik)

# GNU Affero General Public License, Version 3 {{{
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
# }}}

.DELETE_ON_ERROR:

unicode := unicode.sh

unicode += DerivedCoreProperties.txt
unicode += PropList.txt
unicode += JavaScript.txt

files := 

all: NotLineTerminator.l UnicodeIDStart.l UnicodeIDContinue.l IdentifierStart.h IdentifierContinue.h

%.txt:
	wget -qc http://www.unicode.org/Public/UCD/latest/ucd/$@

files += NotLineTerminator.l
NotLineTerminator.l: unicode-l.py
	printf '80..2027\n202a..10ffff\n' | ./unicode-l.py NotLineTerminator >$@

files += UnicodeIDStart.l
UnicodeIDStart.l: $(unicode) unicode-l.py
	./unicode.sh ID_Start DerivedCoreProperties.txt Other_ID_Start PropList.txt | ./unicode-l.py UnicodeIDStart >$@

files += UnicodeIDContinue.l
UnicodeIDContinue.l: $(unicode) unicode-l.py
	./unicode.sh ID_Continue DerivedCoreProperties.txt Other_ID_Continue PropList.txt | ./unicode-l.py UnicodeIDContinue >$@

files += IdentifierStart.h
IdentifierStart.h: $(unicode) unicode-c.sh
	./unicode.sh ID_Start DerivedCoreProperties.txt Other_ID_Start PropList.txt JavaScript_ID_Start JavaScript.txt | ./unicode-c.sh IdentifierStart >$@

files += IdentifierContinue.h
IdentifierContinue.h: $(unicode) unicode-c.sh
	./unicode.sh ID_Continue DerivedCoreProperties.txt Other_ID_Continue PropList.txt JavaScript_ID_Continue JavaScript.txt | ./unicode-c.sh IdentifierContinue >$@

clean:
	rm -f $(files)

.PHONY: all clean
