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

my %maxvaluemap = (
	double_fp => "1.8e+308",
	float_fp => "3.402823e+38",
	i64_unsigned => "18446744073709551616",
	i64_signed => "9223372036854775808",
	i32_unsigned => "4294967296",
	i32_signed => "2147483648",
	i16_unsigned => "65536",
	i16_signed => "32768",
	i8_unsigned => "256",
	i8_signed => "128",
	i1_bool => "1"
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
		return "{3} = load i8, i8* {1}, align 1\\n{2} = trunc i8 {3} to i1\\n";
	}
	else
	{
		return "{2} = load $llvmtype, $llvmtype* {1} align $alignmap{$llvmtype}\\n";
	}
}

sub store_constructor {
	my $llvmtype = $_[0];
	if ($llvmtype eq "i1")
	{
		return "{3} = zext i1 {2} to i8\\nstore i8 {3}, i8* {1}, align 1\\n";
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
			if ($sgn_in eq "bool")
			{
				return "{3} = load i8, i8* {1}, align 1\\n{4} = trunc i8 {3} to i1\\n{5} = zext i1 {4} to i32\\n{2} = sitofp i32 {5} to $llvmtype_out\\n";
			}
			if ($sgn_in eq "fp" && typeSize( $llvmtype_out) > typeSize( $llvmtype_in))  {$convop = "fpext";}
			elsif ($sgn_in eq "fp" && typeSize( $llvmtype_out) < typeSize( $llvmtype_in))  {$convop = "fptrunc";}
			elsif ($sgn_in eq "signed") {$convop = "sitofp";}
			elsif ($sgn_in eq "unsigned") {$convop = "uitofp";}
		}
		elsif ($sgn_in eq "fp")
		{
			if ($sgn_out eq "bool")
			{
				return "{3} = load $llvmtype_in, $llvmtype_in* {1}, align $alignmap{ $llvmtype_in}\\n{2} = fcmp une $llvmtype_in {3}, 0.000000e+00\\n";
			}
			if ($sgn_out eq "signed") {$convop = "fptosi";}
			elsif ($sgn_out eq "unsigned") {$convop = "fptoui";}
		}
		else
		{
			if ($sgn_out eq "bool")
			{
				return "{3} = load $llvmtype_in, $llvmtype_in* {1}, align $alignmap{ $llvmtype_in}\\n{2} = cmp ne $llvmtype_in {3}, 0\\n";
			}
			if (typeSize( $llvmtype_out) < typeSize( $llvmtype_in)) {$convop = "trunc";}
			elsif ($sgn_in eq "bool") {$convop = "zext";}
			elsif ($sgn_in eq "unsigned") {$convop = "zext";}
			elsif ($sgn_in eq "signed") {$convop = "sext";}
		}
		return "{3} = load $llvmtype_in, $llvmtype_in* {1}, align $alignmap{ $llvmtype_in}\\n{2} = $convop $llvmtype_in {3} to $llvmtype_out\\n";
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
	if ($sgn eq "bool") {
		$llvmtype_storage = "i8";
	}
	print "\t\tdef_local = \"{1} = alloca $llvmtype_storage, align $alignmap{$llvmtype}\\n\",\n";
	print "\t\tdef_local_val = \"{1} = alloca $llvmtype_storage, align $alignmap{$llvmtype}\\n" . store_constructor($llvmtype) . "\",\n";
	print "\t\tdef_global = \"{1} = internal global $llvmtype_storage $defaultmap{ $llvmtype}, align $alignmap{$llvmtype}\\n\",\n";
	print "\t\tdef_global_val = \"{1} = internal global $llvmtype_storage {2}, align $alignmap{$llvmtype}\\n\",\n";
	print "\t\tdefault = \"" . $defaultmap{ $llvmtype} . "\",\n";
	print "\t\tclass = \"$sgn\",\n";
	print "\t\tmaxvalue = \"" . $maxvaluemap{ $llvmtype . "_" . $sgn } . "\",\n";
	print "\t\tstore = \"" . store_constructor( $llvmtype) . "\",\n";
	print "\t\tload = \"" . load_constructor( $llvmtype) . "\",\n";
	print "\t\tadvance = {";
	if ($sgn eq "fp" || $sgn eq "signed" || $sgn eq "unsigned")
	{
		my $oi = 0;
		foreach my $operand (@content)
		{
			my ($op_typename, $op_llvmtype, $op_sgn) = split( /\t/, $operand);
			if ($typename ne $op_typename && $alignmap{$op_llvmtype} >= $alignmap{$llvmtype})
			{
				if (($sgn ne "signed" && $sgn ne "unsigned") || $sgn ne $op_sgn)
				{
					if ($oi++ > 0) {print ", ";}
					print "\"$op_typename\"";
				}
			}
		}
	}
	print "},\n";
	print "\t\tconv = {";
	my $oi = 0;
	foreach my $operand (@content)
	{
		my ($op_typename, $op_llvmtype, $op_sgn) = split( /\t/, $operand);
		if ($op_typename ne $typename)
		{
			if ($oi++ > 0) {print ",\n";} else {print "\n";}
			my $cnv = load_conv_constructor( $llvmtype, $sgn, $op_llvmtype, $op_sgn);
			my $weight = 0.0;
			if ($alignmap{$llvmtype} < $alignmap{$op_llvmtype})
			{
				$weight += 0.1;
			}
			if (($sgn eq "signed" && $op_sgn eq "unsigned") || ($sgn eq "unsigned" && $op_sgn eq "signed"))
			{
				$weight += 0.1;
			}
			print "\t\t\t[\"$op_typename\"] = {fmt=\"" . $cnv . "\", weight=$weight}";
		}
	}
	print "},\n";
	print "\t\tunop = {\n";
	if ($sgn eq "bool")
	{
		print "\t\t\t[\"\!\"] = \"{2} = xor $llvmtype {1}, true\\n\"";
	}
	elsif ($sgn eq "unsigned")
	{
		print "\t\t\t[\"\!\"] = \"{2} = xor $llvmtype {1}, -1\\n\"";
	}
	elsif ($sgn eq "signed")
	{
		print "\t\t\t[\"-\"] = \"{2} = sub $llvmtype 0, {1}\\n\"";
	}
	elsif ($sgn eq "fp")
	{
		print "\t\t\t[\"-\"] = \"{2} = fneg $llvmtype {1}\\n\"";
	}
	else
	{
		die "Unknown sgn '$sgn' (3rd column in typelist)";
	}
	print "},\n";
	print "\t\tbinop = {";
	my @ops = ();
	if ($sgn eq "bool")
	{
		@ops = ("[\"&&\"]=and", "[\"||\"]=or");
	}
	elsif ($sgn eq "unsigned")
	{
		@ops = ("[\"+\"]=add nuw", "[\"-\"]=add nsw", "[\"*\"]=mul nsw", "[\"/\"]=udiv", "[\"%\"]=urem",
			"[\"<<\"]=shl", "[\">>\"]=lshr",
			"[\"&\"]=and", "[\"|\"]=or", "[\"^\"]=xor");
	}
	elsif ($sgn eq "signed")
	{
		@ops = ("[\"+\"]=add nsw", "[\"-\"]=sub nsw", "[\"*\"]=mul nsw", "[\"/\"]=sdiv", "[\"%\"]=srem",
			"[\"<<\"]=shl nsw", "[\">>\"]=ashr");
	}
	elsif ($sgn eq "fp")
	{
		@ops = ("[\"+\"]=fadd", "[\"-\"]=fsub", "[\"*\"]=fmul", "[\"/\"]=fdiv", "[\"%\"]=frem");
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
		print "\t\t\t$name = \"{3} = $llvm_name $llvmtype {1}, {2}\\n\"";
	}
	print "}\n";
	print "\t}";
}
print "\n}\n";

