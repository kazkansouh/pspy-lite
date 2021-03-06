#
# Copyright (c) 2020 Karim Kanso. All Rights Reserved.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

# to enable debug statements: make CFLAGS=-DDEBUG
# to build standard production: make LDFLAGS=-s
# to build stripped static: make LDFLAGS="-static -s"

override CFLAGS+=-Wall -Wpedantic -Werror -Os -std=gnu11

OBJECTS=trigger.o scan.o

NAME=pspy-lite

$(NAME): $(OBJECTS)

.PHONY: clean
clean:
	rm -f $(NAME) $(OBJECTS)
