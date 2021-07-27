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
	open( RINFILE, "$infile") or die "failed to read from file $infile: $!";
	while (<RINFILE>)
	{
		$rt .= $_;
	}
	close( RINFILE);
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
	my $grepout = $_[3];

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
				my $exampleSrc = "";
				if (defined $grepout)
				{
					open( XMPLFILE, "$exampleFile") or die("Could not open file $exampleFile for reading: $!");
					while (<XMPLFILE>)  {
						if (/$grepout/)
						{}
						else
						{
							if ($exampleSrc eq "" && $_ eq "\n") {} else {$exampleSrc .= $_;}
						}
					}
					close( FILE);
				}
				else
				{
					$exampleSrc = readFile( "$exampleFile");
				}
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
readExamples( "intro_", "examples/intro");
readIncludes( "intro_", "examples/intro", "prg");
readIncludes( "intro_", "examples/intro", "g");
readIncludes( "intro_section_", "examples/intro/sections", "lua", " require ");
readIncludes( "intro_ast_", "examples/intro/ast", "lua", " require ");
readExamples( "tutorial_", "examples/intro/tutorial");
readIncludes( "tutorial_", "examples/intro/tutorial", "txt");

sub stripEoln
{
	my $rt = $_[0];
	chop( $rt);
	return $rt;
}

sub substVariables
{
	my $line = $_[0];
	if ($line =~ m/^([^\$]*)[\$][:]([A-Za-z0-9_]+)([^:A-Za-z0-9_].*)$/)
	{
		my $rt = $1;
		my $var = $2;
		my $rest = $3;
		if ($var eq "VERSION")
		{
			$rt .= readFile( $var) . substVariables( $rest);
		}
		elsif ($var eq "VERSION_MAJOR")
		{
			my $version = readFile( "VERSION");
			if ($version =~ m/([0-9]+)[.]([0-9]+)/)
			{
				$rt .= $1 . substVariables( $rest);
			}
			else
			{
				$rt .= "0" . substVariables( $rest);
			}
		}
		elsif ($var eq "VERSION_MINOR")
		{
			my $version = readFile( "VERSION");
			if ($version =~ m/([0-9]+)[.]([0-9]+)/)
			{
				$rt .= $2 . substVariables( $rest);
			}
			else
			{
				$rt .= "0" . substVariables( $rest);
			}
		}
		elsif  ($var eq "VERSION_PATCH")
		{
			my $ln = `git log | grep -n Version | head -n 1 | awk -F ':' '{print \$1}'`;
			chop( $ln);
			my $patch = `git log | head -n $ln | grep commit | wc -l`;
			$patch = int($patch) - 1;
			$rt .= "$patch" . substVariables( $rest);
		}
		elsif ($var eq "LLVM_VERSION")
		{
			$rt .= stripEoln(`llvm-config --version`) . substVariables( $rest);
		}
		else
		{
			die "Unknown variable '$var'";
		}
		return $rt;
	}
	elsif ($line =~ m/^([^\$]*)[\$]([A-Z]+)[:]([A-Za-z0-9_]+)(.*)$/)
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
		if (not defined $substval) {die "Unknown substitution $dom : $var\n";}
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
	open( SINFILE, $infile) or die("Could not open file $infile for reading: $!");
	while (<SINFILE>)  {
		$rt .= substVariables( $_);
	}
	close( SINFILE);
	return $rt;
}

sub processDocDir
{
	my $docdir = $_[0];
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
}

sub processSubstFile
{
	my $outFile = $_[0];
	my $inFile = "$outFile.in";
	
	writeFile( $outFile, substVariablesFile( $inFile));
}

processDocDir( "doc");
processDocDir( ".");
processSubstFile( "src/version.hpp");

