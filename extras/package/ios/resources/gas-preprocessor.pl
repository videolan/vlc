#!/usr/bin/env perl
# by David Conrad
# This code is licensed under GPLv2 or later; go to gnu.org to read it
#  (not that it much matters for an asm preprocessor)
# usage: set your assembler to be something like "perl gas-preprocessor.pl gcc"
use strict;

# Apple's gas is ancient and doesn't support modern preprocessing features like
# .rept and has ugly macro syntax, among other things. Thus, this script
# implements the subset of the gas preprocessor used by x264 and ffmpeg
# that isn't supported by Apple's gas.

my @gcc_cmd = @ARGV;
my @preprocess_c_cmd;

if (grep /\.c$/, @gcc_cmd) {
    # C file (inline asm?) - compile
    @preprocess_c_cmd = (@gcc_cmd, "-S");
} elsif (grep /\.[sS]$/, @gcc_cmd) {
    # asm file, just do C preprocessor
    @preprocess_c_cmd = (@gcc_cmd, "-E");
} else {
    die "Unrecognized input filetype";
}
@gcc_cmd = map { /\.[csS]$/ ? qw(-x assembler -) : $_ } @gcc_cmd;
@preprocess_c_cmd = map { /\.o$/ ? "-" : $_ } @preprocess_c_cmd;

open(ASMFILE, "-|", @preprocess_c_cmd) || die "Error running preprocessor";

my $current_macro = '';
my $macro_level = 0;
my %macro_lines;
my %macro_args;
my %macro_args_default;

my @pass1_lines;

# pass 1: parse .macro
# note that the handling of arguments is probably overly permissive vs. gas
# but it should be the same for valid cases
while (<ASMFILE>) {
    # comment out unsupported directives
    s/\.type/@.type/x;
    s/\.func/@.func/x;
    s/\.endfunc/@.endfunc/x;
    s/\.ltorg/@.ltorg/x;
    s/\.size/@.size/x;
    s/\.fpu/@.fpu/x;

    # the syntax for these is a little different
    s/\.global/.globl/x;
    # also catch .section .rodata since the equivalent to .const_data is .section __DATA,__const
    s/(.*)\.rodata/.const_data/x;
    s/\.int/.long/x;
    s/\.float/.single/x;

    # catch unknown section names that aren't mach-o style (with a comma)
    if (/.section ([^,]*)$/) {
        die ".section $1 unsupported; figure out the mach-o section name and add it";
    }

    parse_line($_);
}

sub parse_line {
    my $line = @_[0];

    if (/\.macro/) {
        $macro_level++;
        if ($macro_level > 1 && !$current_macro) {
            die "nested macros but we don't have master macro";
        }
    } elsif (/\.endm/) {
        $macro_level--;
        if ($macro_level < 0) {
            die "unmatched .endm";
        } elsif ($macro_level == 0) {
            $current_macro = '';
            return;
        }
    }

    if ($macro_level > 1) {
        push(@{$macro_lines{$current_macro}}, $line);
    } elsif ($macro_level == 0) {
        expand_macros($line);
    } else {
        if (/\.macro\s+([\d\w\.]+)\s*(.*)/) {
            $current_macro = $1;

            # commas in the argument list are optional, so only use whitespace as the separator
            my $arglist = $2;
            $arglist =~ s/,/ /g;

            my @args = split(/\s+/, $arglist);
            foreach my $i (0 .. $#args) {
                my @argpair = split(/=/, $args[$i]);
                $macro_args{$current_macro}[$i] = $argpair[0];
                $argpair[0] =~ s/:vararg$//;
                $macro_args_default{$current_macro}{$argpair[0]} = $argpair[1];
            }
            # ensure %macro_lines has the macro name added as a key
            $macro_lines{$current_macro} = [];

        } elsif ($current_macro) {
            push(@{$macro_lines{$current_macro}}, $line);
        } else {
            die "macro level without a macro name";
        }
    }
}

sub expand_macros {
    my $line = @_[0];

    if (/\.purgem\s+([\d\w\.]+)/) {
        delete $macro_lines{$1};
        delete $macro_args{$1};
        delete $macro_args_default{$1};
        return;
    }

    if ($line =~ /(\S+:|)\s*([\w\d\.]+)\s*(.*)/ && exists $macro_lines{$2}) {
        push(@pass1_lines, $1);
        my $macro = $2;

        # commas are optional here too, but are syntactically important because
        # parameters can be blank
        my @arglist = split(/,/, $3);
        my @args;
        foreach (@arglist) {
            my @whitespace_split = split(/\s+/, $_);
            if (!@whitespace_split) {
                push(@args, '');
            } else {
                foreach (@whitespace_split) {
                    if (length($_)) {
                        push(@args, $_);
                    }
                }
            }
        }

        my %replacements;
        if ($macro_args_default{$macro}){
            %replacements = %{$macro_args_default{$macro}};
        }

        # construct hashtable of text to replace
        foreach my $i (0 .. $#args) {
            my $argname = $macro_args{$macro}[$i];

            if ($args[$i] =~ m/=/) {
                # arg=val references the argument name
                # XXX: I'm not sure what the expected behaviour if a lot of
                # these are mixed with unnamed args
                my @named_arg = split(/=/, $args[$i]);
                $replacements{$named_arg[0]} = $named_arg[1];
            } elsif ($i > $#{$macro_args{$macro}}) {
                # more args given than the macro has named args
                # XXX: is vararg allowed on arguments before the last?
                $argname = $macro_args{$macro}[-1];
                if ($argname =~ s/:vararg$//) {
                    $replacements{$argname} .= ", $args[$i]";
                } else {
                    die "Too many arguments to macro $macro";
                }
            } else {
                $argname =~ s/:vararg$//;
                $replacements{$argname} = $args[$i];
            }
        }

        # apply replacements as regex
        foreach (@{$macro_lines{$macro}}) {
            my $macro_line = $_;
            # do replacements by longest first, this avoids wrong replacement
            # when argument names are subsets of each other
            foreach (reverse sort {length $a <=> length $b} keys %replacements) {
                $macro_line =~ s/\\$_/$replacements{$_}/g;
            }
            $macro_line =~ s/\\\(\)//g;     # remove \()
            parse_line($macro_line);
        }
    } else {
        push(@pass1_lines, $line);
    }
}

close(ASMFILE) or exit 1;
open(ASMFILE, "|-", @gcc_cmd) or die "Error running assembler";

my @sections;
my $num_repts;
my $rept_lines;

my %literal_labels;     # for ldr <reg>, =<expr>
my $literal_num = 0;

# pass 2: parse .rept and .if variants
# NOTE: since we don't implement a proper parser, using .rept with a
# variable assigned from .set is not supported
foreach my $line (@pass1_lines) {
    # textual comparison .if
    # this assumes nothing else on the same line
    if ($line =~ /\.ifnb\s+(.*)/) {
        if ($1) {
            $line = ".if 1\n";
        } else {
            $line = ".if 0\n";
        }
    } elsif ($line =~ /\.ifb\s+(.*)/) {
        if ($1) {
            $line = ".if 0\n";
        } else {
            $line = ".if 1\n";
        }
    } elsif ($line =~ /\.ifc\s+(.*)\s*,\s*(.*)/) {
        if ($1 eq $2) {
            $line = ".if 1\n";
        } else {
            $line = ".if 0\n";
        }
    }

    # handle .previous (only with regard to .section not .subsection)
    if ($line =~ /\.(section|text|const_data)/) {
        push(@sections, $line);
    } elsif ($line =~ /\.previous/) {
        if (!$sections[-2]) {
            die ".previous without a previous section";
        }
        $line = $sections[-2];
        push(@sections, $line);
    }

    # handle ldr <reg>, =<expr>
    if ($line =~ /(.*)\s*ldr([\w\s\d]+)\s*,\s*=(.*)/) {
        my $label = $literal_labels{$3};
        if (!$label) {
            $label = ".Literal_$literal_num";
            $literal_num++;
            $literal_labels{$3} = $label;
        }
        $line = "$1 ldr$2, $label\n";
    } elsif ($line =~ /\.ltorg/) {
        foreach my $literal (keys %literal_labels) {
            $line .= "$literal_labels{$literal}:\n .word $literal\n";
        }
        %literal_labels = ();
    }

    # @l -> lo16()  @ha -> ha16()
    $line =~ s/,\s+([^,]+)\@l(\s)/, lo16($1)$2/g;
    $line =~ s/,\s+([^,]+)\@ha(\s)/, ha16($1)$2/g;

    if ($line =~ /\.rept\s+(.*)/) {
        $num_repts = $1;
        $rept_lines = "\n";

        # handle the possibility of repeating another directive on the same line
        # .endr on the same line is not valid, I don't know if a non-directive is
        if ($num_repts =~ s/(\.\w+.*)//) {
            $rept_lines .= "$1\n";
        }
        $num_repts = eval($num_repts);
    } elsif ($line =~ /\.endr/) {
        for (1 .. $num_repts) {
            print ASMFILE $rept_lines;
        }
        $rept_lines = '';
    } elsif ($rept_lines) {
        $rept_lines .= $line;
    } else {
        print ASMFILE $line;
    }
}

print ASMFILE ".text\n";
foreach my $literal (keys %literal_labels) {
    print ASMFILE "$literal_labels{$literal}:\n .word $literal\n";
}

close(ASMFILE) or exit 1;
