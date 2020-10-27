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
	i8 => 1,
	i1 => 1
);

my %defaultmap = (
	double => "0.00000",
	float => "0.00000",
	i64 => "0",
	i32 => "0",
	i16 => "0",
	i8 => "0",
	i1 => "false"
);

sub LLVM
{
	my $rt = "";
	my @arg = @_;
	foreach my $aa( @arg)
	{
		if ($rt ne "") {$rt .= ", ";}
		$aa =~ s/['"\\\$\%\@]/\\$1/g;
		$rt .= '"' . $aa . '"';
	}
	return $rt;
}

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
	if ($llvmtype eq "i1")
	{
		my $rt = "";
		$rt .= "{" . LLVM( "/R1", " = load i8, i8* ", $in, ", align 1" ) . "}, ";
		$rt .= "{" . LLVM( $out, " = trunc i8 ", "/R1", " to i1" ) . "}";
		return $rt;
	}
	else
	{
		return "{" . LLVM( $out, " = load $llvmtype, $llvmtype* ", $in, ", align $alignmap{$llvmtype}" ) . "}";
	}
}

sub store_constructor {
	my $llvmtype = $_[0];
	my $adr = $_[1];
	my $in = $_[2];
	if ($llvmtype eq "i1")
	{
		my $rt = "";
		$rt .= "{" . LLVM( "/R1", " = zext i1 ", $in, " to i8" ) . "}, ";
		$rt .= "{" . LLVM( "store i8 ", "/R1", ", i8* ", $adr, ", align 1" ) . "}";
		return $rt;
	}
	else
	{
		return "{" . LLVM( "store $llvmtype ", $in, ", $llvmtype* ", $adr, ", align $alignmap{$llvmtype}" ) . "}";
	}
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
		return load_constructor( $llvmtype_in, "/R1", $in);
	}
	else
	{
		if ($sgn_out eq "fp")
		{
			if ($llvmtype_in eq "i1")
			{
				my $rt = load_constructor( "i1", "/R1", $in) . ", ";
				$rt .= "{" . LLVM( "/R2", " = zext i1 ", "/R1", " to i32" ) . "},";
				$rt .= "{" . LLVM( $out, " = sitofp i32 ", "/R2", " to $llvmtype_out" ) . "}";
				return $rt;
			}
			if ($sgn_in eq "fp" && typeSize( $llvmtype_out) > typeSize( $llvmtype_in))  {$convop = "fpext";}
			elsif ($sgn_in eq "fp" && typeSize( $llvmtype_out) < typeSize( $llvmtype_in))  {$convop = "fptrunc";}
			elsif ($sgn_in eq "signed") {$convop = "sitofp";}
			elsif ($sgn_in eq "unsigned") {$convop = "uitofp";}
		}
		elsif ($sgn_in eq "fp")
		{
			if ($llvmtype_out eq "i1")
			{
				my $rt = load_constructor( $llvmtype_in, "/R1", $in) . ", ";
				$rt .= "{" . LLVM( $out, " = fcmp une $llvmtype_in ", "/R1", ", 0.000000e+00" ) . "}";
				return $rt;
			}
			if ($sgn_out eq "signed") {$convop = "fptosi";}
			elsif ($sgn_out eq "unsigned") {$convop = "fptoui";}
		}
		else
		{
			if ($llvmtype_out eq "i1")
			{
				my $rt = load_constructor( $llvmtype_in, "/R1", $in) . ", ";
				$rt .= "{" . LLVM( $out, " = icmp ne $llvmtype_in ", "/R1", ", 0" ) . "}";
				return $rt;
			}
			if (typeSize( $llvmtype_out) < typeSize( $llvmtype_in)) {$convop = "trunc";}
			elsif ($sgn_in eq "unsigned") {$convop = "zext";}
			elsif ($sgn_in eq "signed") {$convop = "sext";}
		}
		my $rt = load_constructor( $llvmtype_in, "/R1", $in) . ", ";
		$rt .= "{" . LLVM( $out, " = $convop $llvmtype_in ", "/R1", " to $llvmtype_out" ) . "}";
		return $rt;
	}
}

sub typeSize {
	my $llvmtype = $_[0];
	return int($alignmap{ $llvmtype});
}

print "return {\n";
print "constructor = {\n";
print "\t-- /IN variable address (\@name) or register (\%id)\n";
print "\t-- /OUT output register (\%id)\n";
print "\t-- /ADR variable address (\@name) or register (\%id)\n";
print "\t-- /VAL constant value of a matching LLVM type\n";
my @content = readNonEmptyLinesFromFile( $typelist);
my $linecnt = 0;
foreach my $line (@content)
{
	if ($linecnt++ > 0) {print ",\n";} else {print "\n";}
	my ($typename, $llvmtype, $sgn) = split( /\t/, $line);
	print "\t$typename = {\n";
	print "\t\tdef_local  = { { " . LLVM( "/OUT", " = alloca $llvmtype, align $alignmap{$llvmtype}" ) . " }},\n";
	print "\t\tdef_global = { { " . LLVM( "/ADR", " = internal global $llvmtype ", "/VAL", ", align $alignmap{$llvmtype}" ) . " }},\n";
	print "\t\tdefault = \"" . $defaultmap{ $llvmtype} . "\",\n";
	print "\t\tstore = { " . store_constructor( $llvmtype, "/ADR", "/IN") . "},\n";
	print "\t\tload = { " . load_constructor( $llvmtype, "/OUT", "/IN") . "},\n";
	print "\t\tconv = {";
	my $oi = 0;
	foreach my $operand (@content)
	{
		if ($oi++ > 0) {print ",\n";} else {print "\n";}
		my ($op_typename, $op_llvmtype, $op_sgn) = split( /\t/, $operand);
		my $cnv = load_conv_constructor( $llvmtype, "/OUT", $sgn, $op_llvmtype, "/IN", $op_sgn);
		print "\t\t\t_$op_typename = { " . $cnv . "}";
	}
	print "\n\t\t},\n";
	print "\t\tunop = {\n";
	if ($llvmtype eq "i1")
	{
		print "\t\t\t_not = { " . LLVM( "/OUT", " = xor $llvmtype ", "/IN", ", true") . " }\n";
	}
	elsif ($sgn eq "unsigned")
	{
		print "\t\t\t_not = { " . LLVM( "/OUT", " = xor $llvmtype ", "/IN", ", -1") . " }\n";
	}
	elsif ($sgn eq "signed")
	{
		print "\t\t\t_neg = { " . LLVM( "/OUT", " = sub $llvmtype 0, ", "/IN") . " }\n";
	}
	elsif ($sgn eq "fp")
	{
		print "\t\t\t_neg = { " . LLVM( "/OUT", " = fneg $llvmtype 0, ", "/IN") . " }\n";
	}
	else
	{
		die "Unknown sgn '$sgn' (3rd column in typelist)";
	}
	print "\t\t},\n";
	print "\t\tbinop = {";
	my @ops = ();
	if ($llvmtype eq "i1")
	{
		@ops = ("and=and", "or=or", "xor=xor");
	}
	elsif ($sgn eq "unsigned")
	{
		@ops = ("add=add nuw", "sub=add nsw", "mul=mul nsw", "div=udiv", "mod=urem", "shl=shl", "shr=lshr", "and=and", "or=or", "xor=xor");
	}
	elsif ($sgn eq "signed")
	{
		@ops = ("add=add nsw", "sub=sub nsw", "mul=mul nsw", "div=sdiv", "mod=srem", "shl=shl nsw", "shr=ashr");
	}
	elsif ($sgn eq "fp")
	{
		@ops = ("add=fadd", "sub=fsub", "mul=fmul", "div=fdiv", "mod=frem");
	}
	else
	{
		die "Unknown sgn '$sgn' (3rd column in typelist)";
	}
	$oi = 0;
	foreach my $op( @ops)
	{
		if ($oi++ > 0) {print ",\n";} else {print "\n";}
		my ($name,$llvm_name) = split /=/, $op;
		print "\t\t\t_$name = { " . LLVM( "/OUT", " = $llvm_name $llvmtype, ", "/IN" ) . " }";
	}
	print "\n\t\t}\n";
	print "\t}";
}
print "}}\n";

