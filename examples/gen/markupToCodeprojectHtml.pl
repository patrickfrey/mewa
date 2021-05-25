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
	$content =~ s@(_)([^_]*)(_)@<em>$2</em>@g;
	$content =~ s@(\*\*)([^\*]*)(\*\*)@<strong>$2</strong>@g;
	$content =~ s@[\[](http[^\]]*)[\]][\(]([^\)]*)[\)]@<a href="$2">$1</a>@g;
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
	if ($text eq "") {return;}
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
	printSection( "code", $text);
	print( "</pre>\n");
	print( "</div>\n");
	$text = "";
}

sub closeList {
	my $indentlen = $_[0];
	while ($#listlevel != -1 && $listlevel[ $#listlevel] > $indentlen)
	{
		my $tp = $listtype[ $#listlevel];
		pop( @listlevel);
		pop( @listtype);
		if ($tp == "*" || $tp == "-")
		{
			print( "</ul>");
		}
		else
		{
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
		if ($marker == "*" || $marker == "-")
		{
			print( "<ul>\n");
		}
		else
		{
			print( "<ol>\n");
		}
	}
}

sub processLine {
	my $line = $_[0];
	if ($state eq "TEXT")
	{
		if ($line =~ m/^([#]*)([^#].*)$)
		{
			closeList( -1);
			printText();
			$tag = "h" . length($1);
			my $title = chop($2);
			printSection( $tag, $title);
		}
		elsif ($line =~ m/^[\`]{3,3}([A-Za-z0-9_]*)$/)
		{
			closeList( -1);
			printText();
			$state = "CODE";
			$language = $1;
		}
		elsif ($line =~ m/^(\s)([\*\-])[ ](.*)$/ || $line =~ m/^(\s)([1][\.])[ ](.*)$/ )
		{
			my $indent = $1;
			my $marker = $2;
			my $item = $3;
			$indent =~ s/\t/    /;
			closeList( length($indent));
			openList( length($indent), $marker);
			printSection( "li", substMarkup( encodeHtml( $item)));
		}
		else
		{
			closeList( -1);
			$text .= substMarkup( encodeHtml( $line));
		}
	}
	elsif ($state eq "CODE")
	{
		if ($line =~ m/^[`]{3,3}$/)
		{
			printCode();
			$state = "TEXT";
		}
		elsif ($line =~ m/^[`]/)
		{
			die "Bad line in $state: $line";
		}
		else
		{
			if ($#line > 85) {die "Line too long: $line";}
			$text .= encodeHtml( $line);
		}
	}
	else
	{
		die "Bad state: $state";
	}
}

while (<STDIN>)
{
	processLine( $_);
}

