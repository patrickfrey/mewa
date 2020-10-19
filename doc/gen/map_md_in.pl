#!/usr/bin/perl
use strict;
use warnings;
use Getopt::Long;

my %ERRCODE = ();
my %ERRTEXT = ();
my %EXAMPLE = ();
my %EXAMOUT = ();
my %USED = ();

my $luabin = "/usr/bin/lua";
my $verbose = 0;
GetOptions ("luabin=s"   => \$luabin,    # string
            "verbose|V"  => \$verbose)   # flag
or die("Error in command line arguments\n");

my $hppfile='src/error.hpp';
open( FILE, $hppfile) or die("Could not open file $hppfile for reading: $!");
while (<FILE>)  {
    if (/^\s*([A-Za-z_0-9]+)[=]([0-9]+)/)
    {
        $ERRCODE{ $1} = $2;
        if ($verbose)
        {
            print( STDERR "ERRCODE:$1 => $2\n");
        }
    }
}
close( FILE);

my $cppfile='src/error.cpp';
open( FILE, $cppfile) or die("Could not open file $cppfile for reading: $!");
while (<FILE>)  {
    if (/^\s*case ([A-Za-z_0-9]+):\s*return\s["]([^"]*)["]/)
    {
        $ERRTEXT{ $1} = $2;
        if ($verbose)
        {
            print( STDERR "ERRTEXT:$1 => $2\n");
        }
    }
}
close( FILE);

my $exampledir = "doc/examples";
opendir( my $dirhnd, $exampledir) || die "Can't open $exampledir: $!";
while (readdir $dirhnd) {
    if (/^(.*)[.]lua$/)
    {
        my $exampleId = $1;
        my $exampleFile = "$exampledir/$exampleId.lua";
        my $exampleOut = "build/$exampleId.out";
        my $exampleSrc = "";
        my $output = `$luabin $exampleFile`;
        open( OUT, '>', $exampleOut) or die "failed to write to file $exampleOut: $!";
        print( OUT $output);
        close( OUT);
        open( LUA, $exampleFile) or die "failed to read from file $exampleFile: $!";
        while (<LUA>)
        {
            $exampleSrc .= $_;
        }
        close( LUA);
        $EXAMPLE{ $exampleId} = $exampleSrc;
        $EXAMOUT{ $exampleId} = $output;

        if ($verbose)
        {
            print( STDERR "EXAMPLE:$exampleId => $exampleSrc\n");
            print( STDERR "EXAMOUT:$exampleId => $output\n");
        }
    }
}
closedir $dirhnd;

sub substVariables( $ )
{
    my ($line) = $@;
    if ($line =~ m/^([^$]*)[$]([A-Z]*)[:]([A-Za-z0-9_]*)(.*)$/)
    {
        my $rt = $1;
        my $dom = $2;
        my $var = $3;
        my $rest = $4;
        if ($dom == "ERRCODE")
        {
            our $ERRCODE;
            $rt .= $ERRCODE{ $var} . substVariables( $rest);
            if ($verbose) {print STDERR "SUBST $ERRCODE:$var => $ERRCODE{ $var}\n";}
        }
        elsif ($dom == "ERRTEXT")
        {
            our $ERRTEXT;
            $rt .= $ERRTEXT{ $var} . substVariables( $rest);
            if ($verbose) {print STDERR "SUBST $ERRTEXT:$var => $ERRTEXT{ $var}\n";}
        }
        elsif ($dom == "EXAMPLE")
        {
            our $EXAMPLE;
            $rt .= $EXAMPLE{ $var} . substVariables( $rest);
            if ($verbose) {print STDERR "SUBST $EXAMPLE:$var => $EXAMPLE{ $var}\n";}
        }
        elsif ($dom == "EXAMOUT")
        {
            our $EXAMOUT;
            $rt .= $EXAMOUT{ $var} . substVariables( $rest);
            if ($verbose) {print STDERR "SUBST $EXAMOUT:$var => $EXAMOUT{ $var}\n";}
        }
        else
        {
            die "Unknown variable domain '$dom'";
        }
        our $USED;
        if (exists( $USED{ "$dom:$var" }))
        {
            $USED{ "$dom:$var" } = 1 + $USED{ "$dom:$var" };
        }
        else
        {
            $USED{ "$dom:$var" } = 1;
        }
        return $rt;
    }
    else
    {
        return $line;
    }
}

my $docdir = "doc";
opendir( my $dochnd, $docdir) || die "Can't open $docdir: $!";
while (readdir $dochnd) {
    if (/^(.*)[.]md[.]in$/)
    {
        my $mdId = $1;
        my $mdInFile = "$docdir/$mdId.md.in";
        my $mdOutFile = "$docdir/$mdId.md";

        my $output = "";
        open( INFILE, $mdInFile) or die("Could not open file $mdInFile for reading: $!");
        while (<INFILE>)  {
            $output .= substVariables( $_);
        }
        close( INFILE);
        open( OUTFILE, '>', $mdOutFile) or die("Could not open file $mdInFile for writing: $!");
        print( OUTFILE $output);
        close( OUTFILE);
    }
}
closedir $dochnd;



