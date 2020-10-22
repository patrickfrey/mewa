#!/usr/bin/perl
use strict;
use warnings;
use Getopt::Long;

my $clangbin = "/usr/bin/clang";
my $template = undef;
my $tmpfile = "build/tmp";
my $verbose = 0;
my $permute = 0;
my $assignments = "";


GetOptions (
	"clang=s"       => \$clangbin,    # string, clang compiler binary
	"template|t=s"  => \$template,    # string, template file to use
	"tmpfile|T=s"   => \$tmpfile,     # string, file name (without extension) for temporary outputs
	"assign|A=s"    => \$assignments, # string, list of comma separated assignents used in templates
	"verbose|V"     => \$verbose,     # flag, verbose output
	"permute|P"     => \$permute)     # flag, permute arguments
or die("Error in command line arguments\n");
unless ($template) {die "Error in command line: Mandatory option --template|-T undefined";}

my $callEmitLlvm = "$clangbin -S -emit-llvm";
my $compileLlvm = "llc";

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

sub readLlvmFile {
	my $infile = $_[0];
	my $rt = "";
	open( INFILE, "$infile") or die "failed to read from file $infile: $!";
	while (<INFILE>)
	{
		if (/[;!].*$/)
		{}
		elsif (/source_filename[ ].*/)
		{}
		elsif (/target[ ].*/)
		{}
		elsif (/attributes[ ].*/)
		{}
		else
		{
			$rt .= $_;
		}
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

sub substVariables
{
	my $line = $_[0];
	my $varstr = $_[1];
	my @varlist = split(/,/, $varstr);

	if ($line =~ m/^([^\%]*)[\%]([A-Z]+)([0-9]*)(.*)$/)
	{
		my $rt = $1;
		my $dom = $2;
		my $var = $3;
		my $rest = $4;
		my $substval = undef;

		if ($dom eq "A")
		{
			$substval = $varlist[ int($var)-1];
		}
		else
		{
			my @assignlist = split /,/, $assignments;
			foreach my $aa( @assignlist)
			{
				my ($kk,$vv) = split /=/, $aa;
				if ("$dom$var" eq $kk)
				{
					$substval = $vv;
					last;
				}
			}
		}
		unless ($substval)
		{
			die "Unknown variable domain '$dom'";
		}
		$rt .= $substval . substVariables( $rest . "\n", $varstr);
		if ($verbose) {print STDERR "SUBST $dom:$var => $substval\n";}
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
	my $varlist = $_[1];
	my $rt = "";
	open( INFILE, $infile) or die("Could not open file $infile for reading: $!");
	while (<INFILE>)  {
		$rt .= substVariables( $_, $varlist);
	}
	close( INFILE);
	return $rt;
}

my @elemar = ();
if ($permute)
{
	foreach my $arg (@ARGV) {
		if ($#elemar == -1)
		{
			@elemar = split( /,/, $arg);
		}
		else
		{
			my @next = split( /,/, $arg);
			my @permutation = ();
			foreach my $ee (@elemar) {
				foreach my $nn (@next) {
					push( @permutation, "$ee,$nn");
				}
			}
			@elemar = @permutation;
		}
	}
}
else
{
	foreach my $arg (@ARGV) {
		if ($#elemar == -1)
		{
			@elemar = split( /,/, $arg);
		}
		else
		{
			my @next = split( /,/, $arg);
			if ($#next != $#elemar) {die "Arguments do not have the same cardinality";}
			my $ai = 0;
			for (; $ai <= $#next; ++$ai)
			{
				$elemar[ $ai] .= "," . $next[ $ai];
			}
		}
	}
}

if ($verbose)
{
	foreach my $elem (@elemar)
	{
		print STDERR "Case $elem\n";
	}
}

my @cc_input = ();
my @llvm_output = ();

foreach my $elem (@elemar)
{
	my $content = substVariablesFile( $template, $elem);
	if ($verbose)
	{
		print STDERR "C INPUT:\n" . $content . "\n\n";
	}
	push( @cc_input, "$content");
	my $tag = $elem;
	$tag =~ s/,/-/g;
	$tag =~ s/ /_/g;
	writeFile( "$tmpfile.$tag.cpp", $content);
	`rm $tmpfile.$tag.err 2> /dev/null`;
	`$callEmitLlvm $tmpfile.$tag.cpp -o $tmpfile.$tag.ll > $tmpfile.$tag.err 2>&1`;
	my $errcnt = `cat $tmpfile.$tag.err | wc -l`;
	$errcnt =~ s/\s//;
	if (int($errcnt) != 0)
	{
		if ($verbose)
		{
			print STDERR "LLVM ERRORS $errcnt: $tag\n";
		}
	}
	else
	{
		`rm $tmpfile.$tag.err 2> /dev/null`;
		my $llvm = readLlvmFile( "$tmpfile.$tag.ll");
		writeFile( "$tmpfile.$tag.llr", $llvm);
		push( @llvm_output, $llvm);
		if ($verbose)
		{
			print STDERR "LLVM OUTPUT:\n" . $llvm . "\n\n";
		}
	}
}


