/*
 * Copyright 2008 Chris Young <chris@unsatisfactorysoftware.co.uk>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* This file generates an AmigaOS compliant version string in version.h */

address command 'svn info >t:ns_svn'

if open('tmp','t:ns_svn','R') then do
	do until word(var,1) = "REVISION:"
		var = upper(readln('tmp'))
	end
	dummy = close('tmp')
	address command 'delete t:ns_svn'
end

svnrev = word(var,2)

if open('tmp','desktop/version.c','R') then do
	do until word(var,3) = "NETSURF_VERSION_MAJOR"
		var = upper(readln('tmp'))
	end
	dummy = close('tmp')
end

majorver = compress(word(var,5),";")
date = translate(date('E'),'.','/')

say '/* This file was automatically generated from version.rexx */'
say 'static __attribute__((used)) char verstag[] = "\0$VER: NetSurf' majorver || '.' || svnrev '(' || date || ')\0";'

