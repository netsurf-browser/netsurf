#
# Public suffix C code generator
#
# Copyright 2015 Vincent Sanders <vince@kyllikki.og>
#
# Permission to use, copy, modify, and/or distribute this software for
# any purpose with or without fee is hereby granted, provided that the
# above copyright notice and this permission notice appear in all
# copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
# WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
# AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
# DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
# OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
# TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
# PERFORMANCE OF THIS SOFTWARE.


# This program converts the public suffix list data [1] into a C
#  program with static data representation and acessor function.
#
# The actual data list [2] should be placed in a file effective_tld_names.dat
#
# The C program is written to stdout, the typical 160K input file
#  generates 500K of program and compiles down to a 100K object file
#
# There is a single exported function
#
# const char *getpublicsuffix(const char *hostname)
# 
# This returns the public suffix of the passed hostname or NULL if
#  there was an error processing the hostname. The returned pointer is
#  within the passed hostname so if the returned pointer is the same as
#  hostname the whole hostname is a public suffix otherwise the passed
#  hostname has a private part.
#
# The resulting C file is mearly a conversion of the input data (the
#  added c code is from this source and licenced under the same terms)
#  and imposes no additional copyright above that of the source data
#  file.
#
# Note: The pnode structure is built assuming there will never be more
#  label nodes than can fit in an unsigned 16 bit value (65535) but as
#  there are currently around 7500 nodes there is space for another
#  58,000 before this becomes an issue.
#
# [1] https://publicsuffix.org/
# [2] https://publicsuffix.org/list/effective_tld_names.dat


# debian package for ordered hashes: libtie-ixhash-perl

use strict;
use warnings;
use utf8;
use Tie::IxHash;


sub treesubdom
{
    my ($tldtree_ref, $nodeidx_ref, $strtab_ref, $stridx_ref, $parts_ref) = @_;

    my $domelem = pop @{$parts_ref};
    my $isexception = 0;
    tie my %node, 'Tie::IxHash'; # this nodes hash

    # deal with explicit domain exceptions 
    $isexception = ($domelem =~ s/\A!//);
    if ($isexception != 0) {
	$node{"!"} = {};
	$$nodeidx_ref += 1;
    }

    # Update string table
    if (! exists $strtab_ref->{$domelem}) {
	# add to string table
	$strtab_ref->{$domelem} = $$stridx_ref;
	{
	    use bytes;
	    # update the character count index
	    $$stridx_ref += length($domelem);
	    $$stridx_ref += 1; # terminator
	}
	
    }

    # link new node list into tree
    if (! exists $tldtree_ref->{$domelem}) {
	$tldtree_ref->{$domelem} = \%node;
	$$nodeidx_ref += 1;
    }
    
    # recurse down if there are more parts to the domain
    if (($isexception == 0) && (scalar(@{$parts_ref}) > 0)) {
	treesubdom($tldtree_ref->{$domelem}, $nodeidx_ref, $strtab_ref, $stridx_ref, $parts_ref);
    }
}

sub phexstr
{
    use bytes;

    my ($str) = @_;
    my $ret;

    my @bytes = unpack('C*', $str);

    $ret = $ret . sprintf("0x%02x, ", scalar(@bytes));

    foreach (@bytes) {
	$ret = $ret . sprintf("0x%02x, ", $_);
    }

    return $ret;
}

# generate all the children of a parent node and recurse into each of
#  those updating optidx to point to the next free node
sub calc_pnode 
{
    my ($parent_ref, $strtab_ref, $opidx_ref) = @_;
    my $our_dat;
    my $child_dat = "";
    my $startidx = $$opidx_ref;
    my $lineidx = -1;

    # update the output index to after this node
    $$opidx_ref += scalar keys %$parent_ref;

    # entry block 
    if ($startidx == ($$opidx_ref - 1)) {
	$our_dat = "\n    /* entry " . $startidx . " */\n    ";
    } else {
	$our_dat = "\n    /* entries " . $startidx . " to " . ($$opidx_ref - 1) . " */\n    ";
    }

    # iterate over each child element domain/ref pair
    while ( my ($cdom, $cref) = each(%$parent_ref) ) {
        # make array look pretty by limiting entries per line
	if ($lineidx == 3) {
	    $our_dat .= "\n    ";
	    $lineidx = 0;
	} elsif ($lineidx == -1) {
	    $lineidx = 1;
	} else {
	    $our_dat .= " ";
	    $lineidx += 1;
	}

	$our_dat .= "{ ";
	$our_dat .= $strtab_ref->{$cdom} . ", ";
	my $child_count = scalar keys (%$cref);
	$our_dat .= $child_count . ", ";
	if ($child_count != 0) {
	    $our_dat .= $$opidx_ref;
	    $child_dat .= calc_pnode($cref, $strtab_ref, $opidx_ref);
	} else {
	    $our_dat .= 0;
	}
	$our_dat .= " },";

    }

    return $our_dat . $child_dat;
}

# main
binmode(STDOUT, ":utf8");

my $filename = "effective_tld_names.dat";

open(my $fh, '<:encoding(UTF-8)', $filename)
    or die "Could not open file '$filename' $!";

tie my %tldtree, 'Tie::IxHash'; # node tree
my $nodeidx = 1; # count of nodes allowing for the root node
tie my %strtab, 'Tie::IxHash'; # string table
my $stridx = 0;

# put the wildcard match at 0 in the string table
$strtab{'*'} = $stridx;
$stridx += 2;

# put the invert match at 2 in the string table
$strtab{'!'} = $stridx;
$stridx += 2;

# read each line from prefix data and inject into hash tree
while (my $line = <$fh>) {
    chomp $line;

    if (($line ne "") && ($line !~ /\/\/.*$/)) {

	# print "$line\n";
	my @parts=split("\\.", $line);

	# recusrsive call to build tree from root

	treesubdom(\%tldtree, \$nodeidx, \%strtab, \$stridx, \@parts);
    }
}

# C program header
print <<EOF;
/*
 * Generated with the genpubsuffix tool from effective_tld_names.dat
 */

#include <stdint.h>
#include <string.h>

EOF

# output string table
#
# array of characters each string is prefixed with its length and the
#  node table below directly indexes emtries. As labels cannot be more
#  than 63 characters a byte length is more than sufficient.

print "static const char stab[" . $stridx . "] = {\n";
while ( my ($key, $value) = each(%strtab) ) {
    print "    " . phexstr($key) . "/* " . $key . " " . $value . " */\n";
}
print "};\n\n";

print "enum stab_entities {\n";
print "    STAB_WILDCARD = 0,\n";
print "    STAB_EXCEPTION = 2\n";
print "};\n\n";


# output static node array
#
# The constructed array of nodes has all siblings sequentialy and an
# index/count to its children. This yeilds a very compact data
# structure easily traversable.
#
# Additional flags for * (match all) and ! (exception) are omitted as
# they can be infered by having a node with a label of 0 (*) or 2 (!)
# as the string table has those values explicitly created.

print "struct pnode {\n";
print "    uint32_t label; /* index of domain element in string table */\n";
print "    uint16_t child_count; /* number of children of this node */\n";
print "    uint16_t child_index; /* index of first child node */\n";
print "};\n\n";

my $opidx = 1; # output index of node

print "static const struct pnode pnodes[" . $nodeidx . "] = {\n";

# root node
print "    /* root entry */\n    { 0," . scalar keys(%tldtree) . ", " . $opidx . " },"; 

# all subsequent nodes
print calc_pnode(\%tldtree, \%strtab, \$opidx);
print "\n};\n\n";

# lookup code
print <<EOF;

#define DOMSEP '.'

static int matchlabel(int parent, const char *start, int len)
{
	int clast = pnodes[parent].child_index + pnodes[parent].child_count;
	int cidx; /*child node index */
	int ridx = -1; /* index of match or -1 */

	if (pnodes[parent].child_count != 0) {
		/* there are child nodes present to scan */

		for (cidx = pnodes[parent].child_index; cidx < clast; cidx++) {
			if (pnodes[cidx].label == STAB_WILDCARD) {
				/* wildcard match */
				ridx = cidx;
			} else {
				if ((stab[pnodes[cidx].label] == len) &&
				    (strncasecmp(&stab[pnodes[cidx].label + 1],
						 start,
						 len) == 0)) {

					if ((pnodes[cidx].child_count == 1) &&
					    (pnodes[pnodes[cidx].child_index].label == STAB_EXCEPTION)) {
						/* exception to previous */
						ridx = -1;
					} else {
						ridx = cidx;
					}
					break;
				}
			}
		}
	}
	return ridx;
}

/*
 * Exported public API 
 */
const char *getpublicsuffix(const char *hostname)
{
	int treeidx = 0; /* index to current tree node */
	const char *elem_start;
	const char *elem_end;
	int lab_count = 0;

	/* deal with obviously bad hostname */
	if ((hostname == NULL) ||
	    (hostname[0]) == 0 ||
	    (hostname[0] == DOMSEP)) {
		return NULL;
	}

	/* hostnames are ass backwards and we need to consider elemets
	 * from the end first.
	 */
	 elem_end = hostname + strlen(hostname);
	 /* fqdn have a separator on the end */
	 if (elem_end[-1] == DOMSEP) {
		 elem_end--;
	 }
	 elem_start = elem_end;

	 /* extract the element and check for a match in our tree */
	 for(;;) {
		 /* find the start of the element */
		 while ((elem_start > hostname) && (*elem_start != DOMSEP)) {
			 elem_start--;
		 }
		 if (*elem_start == DOMSEP) {
			 elem_start++;
		 }

		 lab_count++;

		 /* search child nodes for label */
		 treeidx = matchlabel(treeidx, elem_start, elem_end - elem_start);
		 if (treeidx == -1) {
			 break;
		 }

		 if (elem_start == hostname) {
			 /* not valid */
			 return NULL;
		 }

		 elem_end = elem_start - 1;
		 elem_start = elem_end - 1;
	 }

	 /* The public suffix algorithm says: "the domain must match
	  * the public suffix plus one additional label." This
	  * requires there to be at least two labels so we need to
	  * check
	  */
	 if (lab_count == 1) {
		 if (elem_start == hostname) {
			 elem_start = NULL;
		 } else {
			 /* strip the non matching part */
			 elem_start -= 2;
			 while (elem_start > hostname && *elem_start != DOMSEP) {
				 elem_start--;
			 }
			 if (*elem_start == DOMSEP)
				 elem_start++;
		 }
	 }


	 return elem_start;
}


EOF
