#!/usr/bin/perl
use strict;
use warnings;
use Getopt::Long;

my $clangbin = "/usr/bin/clang";
my $template = undef;
my $verbose = 0;
my $permute = 0;

GetOptions ("clang=s"       => \$clangbin,    # string, clang compiler binary
            "template|t=s"  => undef,         # string, template file to use
            "verbose|V"     => \$verbose),    # flag, verbose output
            "permute|P"     => \$permute)     # flag, permute arguments
or die("Error in command line arguments\n");

my $callEmitLlvm = "$clangbin -S -emit-llvm"
my $compileLlvm = "llc"

sub readFile {
    my $infile = $_[0];
    my $rt = "";
    open( INFILE, "$infile") or die "failed to read from file $infile: $!";
    while (<INFILE>)
    {
        $rt .= $_;
    }
    close( INFILE);
    return $rt;
}

sub writeFile {
    my $outfile = $_[0];
    my $content = $_[1];
    open( OUT, '>', $outfile) or die "failed to write to file $outfile: $!";
    print( OUT $content);
    close( OUT);
}

