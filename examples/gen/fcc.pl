#!/usr/bin/perl
use strict;
use warnings;
use Getopt::Long;

my $typelist = "examples/templates/types_language1.txt";
my $verbose = 0;

GetOptions (
	"typelist|t=s"  => \$typelist,    # string, file with list of types
	"verbose|V"     => \$verbose)     # verbose output
or die("Error in command line arguments\n");

my %alignmap = (
	double => 8,
	float => 4,
	i64 => 8,
	i32 => 4,
	i16 => 2,
	i8 => 1
);

my %extmap = (
	signed => "signext",
	unsigned => "zeroext",
	'-' => ""
);

sub readNonEmptyLinesFromFile {
	my $infile = $_[0];
	my @rt = ();
	open( INFILE, "$infile") or die "failed to read from file $infile: $!";
	while (<INFILE>)
	{
		chomp;
		if (/\S+/)
		{
			push( @rt, $_);
		}
	}
	close( INFILE);
	return @rt;
}

print "return {\n";
print "constructor = {\n";
print "\t-- $IN input register (\%id)";
print "\t-- $OUT output register (\%id)";
print "\t-- $ADR variable address (\@name) or register (\%id)";
print "\t-- $VAL constant value of a matching LLVM type";
my @content = readNonEmptyLinesFromFile( $typelist);
foreach my $line (@content)
{
	my ($typename, $llvmtype, $sgn) = split( /\t/, $line);
	print "\t$typename = {\n";
	print "\t\tdef_local  = { \"\$OUT\", \" = alloca $llvmtype, align $alignmap{$llvmtype}\" },\n";
	print "\t\tdef_global = { \"\$ADR\", \" = internal global $llvmtype \", \"\$VAL\", \", align $alignmap{$llvmtype}\" },\n";
	print "\t\tstore = { \"store $llvmtype \", \"\$IN\", \", $llvmtype* \", \"\$ADR\", \", align align $alignmap{$llvmtype}\" },\n";
	print "\t\tload = { \"\$OUT\", \" = load $llvmtype, $llvmtype* \", \"\$ADR", ", align $alignmap{$llvmtype}\" }\n";
	print "\t}\n";
}
print "}}\n";




