#! /usr/bin/perl

# Attributes
$bold = "\033[1m";

# Colors
$white  = "\033[37m";
$yellow  = "\033[33m";
$magenta  = "\033[35m";
$blue  = "\033[34m";
$red  = "\033[31m";
$reset = "\033[0m";

# Combinations
$info   = $white.$bold;
$warn   = $yellow.$bold;
$error  = $red.$bold;
$file   = $magenta.$bold;
$lineno = $blue.$bold;

while(<STDIN>)
{
     $line = $_;
     chomp $line;
     # Skip entering/leaving directories and incomplete lines
     if($line =~ /make\[([0-9]*)\]:.*/ ||
#       $line =~ /.*\s\\$/ ||
        $line =~ /^test\s\-z\s/ ||
        $line =~ /^Making\sclean\sin\s\./ ||
	$line =~ /then\smv\s-f/ ||
	$line =~ /.*make\s\s.*/ ||
        $line =~ /make\s\sall-recursive/ ||
        $line =~ /[A-z0-9-]*ar\s[A-z0-9]*\s([A-z0-9\-_\/\.]*)\s.*/ ||
        $line =~ /^[A-z0-9-]*ranlib\s[A-z0-9-_]*plugin(.*)/ ||
        $line =~ /^touch.*/ ||
        $line =~ /^srcdir=.*/ ||
        $line =~ /^.* (lib[A-z0-9-_]*plugin.so).*/ ||
	$line =~ /^\s*gcc(-.*)?\s-std=.*/ ||
        $line =~ /^\sgcc(-.*)?\s-mmacosx.*/ ||
        $line =~ /^\sg\+\+(-.*)?\s.*/ ||
#    $line =~ /^.*moc.*/ ||
	$line =~ /^.*libtool.*\-o\s(lib.*\.la).*/ ||
        $line =~ /^.*rm\s\-f\s(.*)/ ||
	$line =~ /^rm\s-fr\s(.*)/ ||
	$line =~ /^mv\s-f\s(.*)/ ||
	$line =~ /^ln\s-s\s(.*)/ ||
	$line =~ /^\s*echo\s/ ||
	$line =~ /^mkdir\s/ ||
	$line =~ /^\s*cat\s/ ||
	$line =~ /^grep\s/ ||
	$line =~ /^cd\s/ ||
	$line =~ /^sed\s/ ||
	$line =~ /^bindir=\s/ ||
	$line =~ /^libtool:\s/ ||
	$line =~ /^\/bin\/sh/ ||
	$line =~ /^\/usr\/bin\/moc-qt4/ ||
	$line =~ /^\/usr\/bin\/uic-qt4/ ||
	$line =~ /^creating lib.*/)
     {}
     # Info
     elsif(
	  $line =~ s/^.*\-shared.*(lib.*\.so).*/ LINK    : $1/g ||
          $line =~ s/^.* (lib.*\.so).*/ LINK    : $1/g ||
          $line =~ s/^.* (lib.*\.o)\s\.\/(.*)/ COMPILE : $2/g ||
          $line =~ s/^.*(lib.*\.lo)\s.*/ COMPILE : $1/g ||
          $line =~ s/^.* (lib.*\.o)\s`.*`(.*);\ \\/ COMPILE : $2/ ||
          $line =~ s/.*\-o\s([^\s]*)\s`.*`([^\s]*);.*/ COMPILE : $2/g ||
          $line =~ s/^[A-z0-9-]*ranlib\s(.*)/ RANLIB  : $1/g ||
          $line =~ s/^Making\sall\sin\s(.*)/MAKE     : $1/g ||
          $line =~ s/^Making\sclean\sin\s(.*)/CLEAN  : $1/g  )
     {
	print $info.$line.$reset."\n";
     }
     # Warning
     elsif (
	  $line =~ s/(.*):([0-9]*):\swarning\:(.*)/WARNING : $file$1: $lineno$2: $warn$3/g  ||
          $line =~ s/.*is\sdeprecated.*/WARNING : $line/g )
     {
	print STDERR $warn.$line.$reset."\n";
     }
     # Error
     elsif (
	  $line =~ s/(.*):([0-9]*):\serror\:(.*)/ERROR   : $file$1: $lineno$2: $error$3/g  )
     {
	print STDERR $error.$line.$reset."\n";
     }
     # Print unmatched lines
     else
     {
 	print $line."\n";
     }
}
