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
GetOptions (
	"luabin=s"   => \$luabin,	# string
	"verbose|V"  => \$verbose)	# flag
or die("Error in command line arguments\n");

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

sub readExamples
{
	my $prefix = $_[0];
	my $exampledir = $_[1];
	opendir( my $dirhnd, $exampledir) || die "Can't open $exampledir: $!";
	while (readdir $dirhnd) {
		if (/^(.*)[.]lua$/)
		{
			my $exampleName = "$1";
			my $exampleId = "$prefix$exampleName";
			my $exampleFile = "$exampledir/$exampleName.lua";
			my $exampleOut = "build/$exampleId.out";
			my $exampleSrc = readFile( "$exampleFile");
			my $output = `$luabin $exampleFile`;
			writeFile( $exampleOut, $output);

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
}

sub readIncludes
{
	my $prefix = $_[0];
	my $exampledir = $_[1];
	my $extension = $_[2];

	opendir( my $dirhnd, $exampledir) || die "Can't open $exampledir: $!";
	while (readdir $dirhnd) {
		if (/^(.*)[.](.*)$/)
		{
			my $exampleName = "$1";
			my $ext = "$2";
			if ($ext eq $extension)
			{
				my $exampleId = "$prefix$exampleName";
				my $exampleFile = "$exampledir/$exampleName.$extension";
				my $exampleSrc = readFile( "$exampleFile");

				$EXAMPLE{ $exampleId} = $exampleSrc;
				if ($verbose)
				{
					print( STDERR "EXAMPLE:$exampleId => $exampleSrc\n");
				}
			}
		}
	}
	closedir $dirhnd;
}

readExamples( "", "doc/examples");
readExamples( "tutorial_", "examples/tutorial/examples");
readExamples( "tutorial_", "examples/tutorial");
readIncludes( "tutorial_", "examples/tutorial", "prg");
readIncludes( "tutorial_", "examples/tutorial", "g");
readIncludes( "tutorial_", "examples/tutorial/examples", "txt");

sub substVariables
{
	my $line = $_[0];
	if ($line =~ m/^([^\$]*)[\$]([A-Z]+)[:]([A-Za-z0-9_]+)(.*)$/)
	{
		my $rt = $1;
		my $dom = $2;
		my $var = $3;
		my $rest = $4;
		my $substval = "";

		if ($var =~ m/^(.*)_$/)
		{
			$var = $1;
			$rest = '_' . $rest;
		}
		if ($dom eq "ERRCODE")
		{
			our $ERRCODE; $substval = $ERRCODE{ $var};
		}
		elsif ($dom eq "ERRTEXT")
		{
			our $ERRTEXT; $substval = $ERRTEXT{ $var};
		}
		elsif ($dom eq "EXAMPLE")
		{
			our $EXAMPLE; $substval = $EXAMPLE{ $var};
		}
		elsif ($dom eq "EXAMOUT")
		{
			our $EXAMOUT; $substval = $EXAMOUT{ $var};
		}
		else
		{
			die "Unknown variable domain '$dom'";
		}

		$rt .= $substval . substVariables( $rest . "\n");
		if ($verbose) {print STDERR "SUBST $dom:$var => $substval\n";}

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

sub substVariablesFile
{
	my $infile = $_[0];
	my $rt = "";
	open( INFILE, $infile) or die("Could not open file $infile for reading: $!");
	while (<INFILE>)  {
		$rt .= substVariables( $_);
	}
	close( INFILE);
	return $rt;
}

my $docdir = "doc";
opendir( my $dochnd, $docdir) || die "Can't open $docdir: $!";
while (readdir $dochnd) {
	if (/^(.*)[.]md[.]in$/)
	{
		my $mdId = $1;
		my $mdInFile = "$docdir/$mdId.md.in";
		my $mdOutFile = "$docdir/$mdId.md";

		my $output = substVariablesFile( $mdInFile);
		writeFile( $mdOutFile, $output);
	}
}
closedir $dochnd;



