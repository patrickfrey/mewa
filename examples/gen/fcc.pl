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
	fp => ""
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

sub load_constructor {
	my $llvmtype = $_[0];
	my $out = $_[1];
	my $in = $_[2];
	return "{\"$out\", \" = load $llvmtype, $llvmtype* \", \"$in\", \", align $alignmap{$llvmtype}\"}";
}

sub load_conv_constructor {
	my $llvmtype_out = $_[0];
	my $out = $_[1];
	my $sgn_out = $_[2];
	my $llvmtype_in = $_[3];
	my $in = $_[4];
	my $sgn_in = $_[5];
	my $convop = undef;
	if ($llvmtype_out eq $llvmtype_in)
	{
		return load_constructor( $llvmtype_in, "\$R1", $in);
	}
	else
	{
		if ($sgn_out eq "fp")
		{
			if ($sgn_in eq "fp" && typeSize( $llvmtype_out) > typeSize( $llvmtype_in))  {$convop = "fpext";}
			elsif ($sgn_in eq "fp" && typeSize( $llvmtype_out) < typeSize( $llvmtype_in))  {$convop = "fptrunc";}
			elsif ($sgn_in eq "signed") {$convop = "sitofp";}
			elsif ($sgn_in eq "unsigned") {$convop = "uitofp";}
		}
		elsif ($sgn_in eq "fp")
		{
			if ($sgn_out eq "signed") {$convop = "fptosi";}
			elsif ($sgn_out eq "unsigned") {$convop = "fptoui";}
		}
		else
		{
			if (typeSize( $llvmtype_out) < typeSize( $llvmtype_in)) {$convop = "trunc";}
			elsif ($sgn_in eq "unsigned") {$convop = "zext";}
			elsif ($sgn_in eq "signed") {$convop = "sext";}
		}
		my $rt = load_constructor( $llvmtype_in, "\$R1", $in) . ", ";
		$rt .= "{\"$out\", \" = $convop $llvmtype_in \", \"\$R1\", \" to $llvmtype_out\"}";
		return $rt;
	}
}

sub typeSize {
	my $llvmtype = $_[0];
	return int($alignmap{ $llvmtype});
}

print "return {\n";
print "constructor = {\n";
print "\t-- \$IN variable address (\@name) or register (\%id)\n";
print "\t-- \$OUT output register (\%id)\n";
print "\t-- \$ADR variable address (\@name) or register (\%id)\n";
print "\t-- \$VAL constant value of a matching LLVM type\n";
my @content = readNonEmptyLinesFromFile( $typelist);
foreach my $line (@content)
{
	my ($typename, $llvmtype, $sgn) = split( /\t/, $line);
	print "\t$typename = {\n";
	print "\t\tdef_local  = { { \"\$OUT\", \" = alloca $llvmtype, align $alignmap{$llvmtype}\" }},\n";
	print "\t\tdef_global = { { \"\$ADR\", \" = internal global $llvmtype \", \"\$VAL\", \", align $alignmap{$llvmtype}\" }},\n";
	print "\t\tstore = { { \"store $llvmtype \", \"\$IN\", \", $llvmtype* \", \"\$ADR\", \", align align $alignmap{$llvmtype}\" }},\n";
	print "\t\tload = { " . load_constructor( $llvmtype, "\$OUT", "\$IN") . "}\n";
	print "\t\tconv = {\n";
	foreach my $operand (@content)
	{
		my ($op_typename, $op_llvmtype, $op_sgn) = split( /\t/, $operand);
		my $cnv = load_conv_constructor( $llvmtype, "\$OUT", $sgn, $op_llvmtype, "\$IN", $op_sgn);
		print "\t\t\t$op_typename = { " . $cnv . "}\n";
	}
	print "\t\t}\n";
	print "\t}\n";
}
print "}}\n";




