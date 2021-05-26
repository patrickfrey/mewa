#!/usr/bin/perl
use strict;
use warnings;
use Getopt::Long;

my $docpath = "https://github.com/patrickfrey/mewa/blob/master/doc/";
my $state = "TEXT";
my $section = "CONTENT";
my $language = undef;
my $text = "";
my @listlevel = ();
my @listtype = ();

my $head = "";
my $tail = "";
my $verbose = 0;

GetOptions (
	"head|H=s"      => \$head,        # string, head text of output
	"tail|T=s"      => \$tail,        # string, tail text of output
	"verbose|V"     => \$verbose)     # verbose output
or die("Error in command line arguments\n");

sub encodeHtml {
	my $content = $_[0];
	$content =~ s/[']/\&\#39\;/g;
	$content =~ s/["]/\&quot\;/g;
	$content =~ s/[<]/\&lt\;/g;
	$content =~ s/[>]/\&gt\;/g;
	return $content;
}

sub substMarkup {
	my $content = $_[0];
	$content =~ s@([`]{3,3})([^`]*)([`]{3,3})@<code>$2</code>@g;
	$content =~ s@(_)([^_`,\"\'\.\)\(\[\]]*)(_)@<em>$2</em>@g;
	$content =~ s@(\*\*)([^\*`,\"\'\.\)\(\[\]]*)(\*\*)@<strong>$2</strong>@g;
	$content =~ s@[\[]([^\]]*)[\]][\(](http[^\)]*)[\)]@<a href="$2">$1</a>@g;
	$content =~ s@[\[]([^\]]*)[\]][\(]([^\)]*)[\)]@<a href="$docpath$2">$1</a>@g;
	return $content;
}

sub printSection {
	my $tag = $_[0];
	my $content = $_[1];

	my @tagparts = split /[ ]/, $tag, 2;
	print( "<" . $tag . ">" . $content . "</" . $tagparts[0] . ">\n");
}

sub printText {
	unless ($text =~ m/\S/) {return;}
	if ($state ne "TEXT") {die "Bad call of print text in state $state";}
	printSection( "p", $text);
	$text = "";
}

sub printCode {
	if ($text eq "") {return;}
	if ($state ne "CODE") {die "Bad call of print text in state $state";}
	my $divtag = "div";
	if ($language and $language ne "")
	{
		$divtag .= " class=\"highlight highlight-source-" . lc( $language) . " position snippet-clipboard-content position-relative\"";
	}
	else
	{
		$divtag .= " class=\"snippet-clipboard-content position-relative\"";
	}
	print( "<" . $divtag . ">\n");
	print( "<pre>\n");
	$text =~ s/\n+$//;
	print( $text);
	$text = "";
	print( "</pre>\n");
	print( "</div>\n");
}

sub closeList {
	my $indentlen = $_[0];
	while ($#listlevel != -1 && $listlevel[ $#listlevel] > $indentlen)
	{
		my $tp = $listtype[ $#listlevel];
		pop( @listlevel);
		pop( @listtype);
		if ($tp eq "*" || $tp eq "-")
		{
			print( "</li>");
			print( "</ul>");
		}
		else
		{
			print( "</li>");
			print( "</ol>");
		}
	}
}

sub openList {
	my $indentlen = $_[0];
	my $marker = $_[1];
	if ($#listlevel == -1 || $listlevel[ $#listlevel] < $indentlen)
	{
		push( @listlevel, $indentlen);
		push( @listtype, $marker);
		if ($marker eq "*" || $marker eq "-")
		{
			print( "<ul>\n");
			print( "<li>");
		}
		else
		{
			print( "<ol>\n");
			print( "<li>");
		}
	}
}

sub processLine {
	my $line = $_[0];
	if ($state eq "TEXT")
	{
		my $len;
		my $tag;
		my $title;
		my $indent;
		my $indentlen;
		my $marker;
		my $item;

		if ($line =~ m/^([\#]+)[ ]*([^\#].*)$/)
		{
			$len = length($1);
			$tag = "h$len";
			$title = $2;
			closeList( -1);
			printText();
			printSection( $tag, substMarkup( encodeHtml( $title)));
		}
		elsif ($line =~ m/^[\`]{3,3}([A-Za-z0-9]*)$/)
		{
			closeList( -1);
			printText();
			$state = "CODE";
			$language = $1;
		}
		elsif ($line =~ m/^([\t ]+)([\*\-])[ ](.*)$/ || $line =~ m/^([\t ]+)([1][\.])[ ](.*)$/ )
		{
			printText();
			$indent = $1;
			$marker = $2;
			$item = $3;
			$indent =~ s/\t/    /;
			$indentlen = length($indent);
			if ($#listlevel != -1 && $listlevel[ $#listlevel] == $indentlen)
			{
				print( "</li>\n<li>");
			}
			else
			{
				closeList( $indentlen);
				openList( $indentlen, $marker);
			}
			print( substMarkup( encodeHtml( $item)));
		}
		else
		{
			closeList( -1);
			unless ($line =~ m/\S/) {printText();}
			my $textline = substMarkup( encodeHtml( $line));
			if ($textline =~ m/\S/) {$text .= $textline;}
		}
	}
	elsif ($state eq "CODE")
	{
		if ($line =~ m/^[\`]{3,3}$/)
		{
			printCode();
			$state = "TEXT";
		}
		else
		{
			$line =~ s/[\t]/  /g;
			my $linelen = length($line);
			if ($linelen > 85) {die "Line too long ($linelen): $line";}
			$text .= encodeHtml( $line);
		}
	}
	else
	{
		die "Bad state: $state";
	}
}

sub finishText {
	if ($state eq "TEXT")
	{
		closeList( -1);
		printText();
	}
	elsif ($state eq "CODE")
	{
		die "Code section not closed";
	}
	else
	{
		die "Bad state: $state";
	}
}

if ($head ne "")
{
	print( "$head\n");
}
while (<STDIN>)
{
	processLine( $_);
}
finishText();

if ($tail ne "")
{
	print( "\n$tail\n");
}
closeList( -1);

