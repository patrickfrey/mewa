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
	if ($llvmtype eq "i1")
	{
		return "{3} = load i8, i8* {1}, align 1\\n{2} = trunc i8 {3} to i1";
	}
	else
	{
		return "{2} = load $llvmtype, $llvmtype* {1} align $alignmap{$llvmtype}";
	}
}

sub store_constructor {
	my $llvmtype = $_[0];
	if ($llvmtype eq "i1")
	{
		return "{3} = zext i1 {2} to i8\\nstore i8 {3}, i8* {1}, align 1";
	}
	else
	{
		return "store $llvmtype {2}, $llvmtype* {1}, align $alignmap{$llvmtype}";
	}
}

sub load_conv_constructor {
	my $llvmtype_out = $_[0];
	my $sgn_out = $_[1];
	my $llvmtype_in = $_[2];
	my $sgn_in = $_[3];
	my $convop = undef;
	if ($llvmtype_out eq $llvmtype_in)
	{
		return load_constructor( $llvmtype_in);
	}
	else
	{
		if ($sgn_out eq "fp")
		{
			my $rt = "";
			if ($llvmtype_in eq "i1")
			{
				return "{3} = load i8, i8* {1}, align 1\\n{4} = trunc i8 {3} to i1\\n{5} = zext i1 {4} to i32\\n{2} = sitofp i32 {5} to $llvmtype_out";
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
				return "{3} = load $llvmtype_in, $llvmtype_in* {1}, align $alignmap{ $llvmtype_in}\\n{2} = fcmp une $llvmtype_in {3}, 0.000000e+00";
			}
			if ($sgn_out eq "signed") {$convop = "fptosi";}
			elsif ($sgn_out eq "unsigned") {$convop = "fptoui";}
		}
		else
		{
			if ($llvmtype_out eq "i1")
			{
				return "{3} = load $llvmtype_in, $llvmtype_in* {1}, align $alignmap{ $llvmtype_in}\\n{2} = cmp ne $llvmtype_in {3}, 0";
			}
			if (typeSize( $llvmtype_out) < typeSize( $llvmtype_in)) {$convop = "trunc";}
			elsif ($sgn_in eq "unsigned") {$convop = "zext";}
			elsif ($sgn_in eq "signed") {$convop = "sext";}
		}
		return "{3} = load $llvmtype_in, $llvmtype_in* {1}, align $alignmap{ $llvmtype_in}\\n{2} = $convop $llvmtype_in {3} to $llvmtype_out";
	}
}

sub typeSize {
	my $llvmtype = $_[0];
	return int($alignmap{ $llvmtype});
}

print "return {\n";
my @content = readNonEmptyLinesFromFile( $typelist);
my $linecnt = 0;
foreach my $line (@content)
{
	if ($linecnt++ > 0) {print ",\n";}
	my ($typename, $llvmtype, $sgn) = split( /\t/, $line);
	print "\t$typename = {\n";
	my $llvmtype_storage = $llvmtype;
	if ($llvmtype eq "i1") {
		$llvmtype_storage = "i8";
	}
	print "\t\tdef_local = \"{1} = alloca $llvmtype_storage, align $alignmap{$llvmtype}\",\n";
	print "\t\tdef_local_val = \"{1} = alloca $llvmtype_storage, align $alignmap{$llvmtype}\\n" . store_constructor($llvmtype) . "\",\n";
	print "\t\tdef_global = \"{1} = internal global $llvmtype_storage $defaultmap{ $llvmtype}, align $alignmap{$llvmtype}\",\n";
	print "\t\tdef_global_val = \"{1} = internal global $llvmtype_storage {2}, align $alignmap{$llvmtype}\",\n";
	print "\t\tdefault = \"" . $defaultmap{ $llvmtype} . "\",\n";
	print "\t\tstore = \"" . store_constructor( $llvmtype) . "\",\n";
	print "\t\tload = \"" . load_constructor( $llvmtype) . "\",\n";
	print "\t\tconv = {";
	my $oi = 0;
	foreach my $operand (@content)
	{
		if ($oi++ > 0) {print ",\n";} else {print "\n";}
		my ($op_typename, $op_llvmtype, $op_sgn) = split( /\t/, $operand);
		my $cnv = load_conv_constructor( $llvmtype, $sgn, $op_llvmtype, $op_sgn);
		print "\t\t\t_$op_typename = \"" . $cnv . "\"";
	}
	print "},\n";
	print "\t\tunop = {\n";
	if ($llvmtype eq "i1")
	{
		print "\t\t\t_not = \"{2} = xor $llvmtype {1}, true\"";
	}
	elsif ($sgn eq "unsigned")
	{
		print "\t\t\t_not = \"{2} = xor $llvmtype {1}, -1\"";
	}
	elsif ($sgn eq "signed")
	{
		print "\t\t\t_neg = \"{2} = sub $llvmtype 0, {1}\"";
	}
	elsif ($sgn eq "fp")
	{
		print "\t\t\t_neg = \"{2} = fneg $llvmtype {1}\"";
	}
	else
	{
		die "Unknown sgn '$sgn' (3rd column in typelist)";
	}
	print "},\n";
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
		print "\t\t\t_$name = \"{3} = $llvm_name $llvmtype {1}, {2}\"";
	}
	print "}\n";
	print "\t}";
}
print "\n}\n";

