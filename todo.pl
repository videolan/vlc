#! /usr/bin/perl

$buffer = "";
$description = "";

open FILE, "TODO";

while (<FILE>) {
	$buffer .= $_;
	if (/^Status:/) {
		if(/Todo/) {
			print $buffer;
		} else {
			print "\n".$description;
			print $_;
		}
		$buffer = "";
		$description = "";
	} elsif (/^Description/) {
		$description = $_;
	}
}

close FILE;

