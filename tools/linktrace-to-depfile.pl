#!/usr/bin/perl -w

use strict;
use Cwd qw(abs_path);

my %deps;

while (my $line = <>) {
   chomp $line;
   if ($line =~ /\(([^)]+)/) {
      $deps{abs_path($1)} = 1;
   }
}

my @deps = keys %deps;

print join("\t\\\n\t", @deps), "\n";

