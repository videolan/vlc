#! /usr/bin/perl

$file_input="font.pnm";
$file_output="eutopiabold36.rle";
$border=-1;
$spaceborder=4;

$|=1;

open(INPUT,$file_input) || die "Couldn't open font: $!\n";

$tag=<INPUT>; chop($tag);
if($tag ne "P6")
  { die "Couldn't process image: not a pnm file ($tag)"; }

$comment=<INPUT>;

$dimensions=<INPUT>; chop($dimensions);

if($dimensions =~ /(\d+) (\d+)/)
  { $width=$1; $height=$2; }
else
  { die "Couldn't process image: couldn't get dimensions"; }

$bits=<INPUT>;

print "width $width height $height\n";

for($j=0; $j<$height; $j++)
  { for($i=0; $i<$width; $i++)
      {
        $red[$i][$j]=ord(getc(INPUT));
        $green[$i][$j]=ord(getc(INPUT));
        $blue[$i][$j]=ord(getc(INPUT));
      }
    print ".";
  }

print "\n";

close(INPUT);

open(OUTPUT,">".$file_output) || die "Couldn't open output: $!\n";

# Put header
print OUTPUT pack("C2",0x36,0x05);

# Put font height
print OUTPUT pack("C",$height);

$xstart=0;

# Search for space

$xstart=2;
$x=$xstart; $blank=0;
while($x<$width && !$blank)
  { $blank=1;
    for($y=0; $y<$height; $y++)
      { if($blue[$x][$y]!=255)
          { $blank=0; }
      }
    if(!$blank)
      { $x++; }
  }

$xstart=$x;

$x=$xstart; $blank=1;
while($x<$width && $blank)
  { $blank=1;
    for($y=0; $y<$height; $y++)
      { if($blue[$x][$y]!=255)
          { $blank=0; }
      }
    if($blank)
      { $x++; }
  }
$xend=$x;

$spacewidth=$xend-$xstart+$spaceborder;
$spacewidth=$spacewidth/2;
if($spacewidth==0)
  { $spacewidth=1; }

print "space start=$xstart end=$xend -> width=$spacewidth\n\n";

# Put space character code
print OUTPUT pack("C",32);

# Put space width
print OUTPUT pack("C",$spacewidth);

# Put space RLE data
for($y=0;$y<$height;$y++)
  { print OUTPUT pack("C",1);
    print OUTPUT pack("C",0);
    print OUTPUT pack("C",$spacewidth);
  }

$char=33;

while($xstart<$width)
  {
    print "looking for character $char \"".chr($char)."\"\n";

    $x=$xstart; $blank=1;
    while($x<$width && $blank)
      { $blank=1;
        for($y=0; $y<$height; $y++)
          { if($blue[$x][$y]!=255)
              { $blank=0; }
          }
        if($blank)
          { $x++; }
      }
    $xstart=$x;

    $x=$xstart; $blank=0;
    while($x<$width && !$blank)
      { $blank=1;
        for($y=0; $y<$height; $y++)
          { if($blue[$x][$y]!=255)
              { $blank=0; }
          }
        if(!$blank)
          { $x++; }
      }
    $xend=$x;
    print "start=$xstart end=$xend\n";

    $dstart=$xstart-$border;
    if($dstart < 0)
      { $dstart = 0; }
    $dend=$xend+$border;
    if($dend > $width)
      { $dend = $width; }

    # Put character
    print OUTPUT pack("C",$char);

    # Put character width
    print OUTPUT pack("C",$dend-$dstart);
    
    for($y=0; $y<$height; $y++)
      { $linecode=""; $bytecode=""; $lastcolour=-1; $count=0;
        for($x=$dstart; $x<$dend; $x++)
          {
            # Transparent background
            $c=":"; $colour=0;

            # Anti-aliased foreground 
            if($blue[$x][$y]<255 && $red[$x][$y]>0)
              { $c="+"; $colour=1; }

            # Solid foreground
            if($blue[$x][$y]==255 && $red[$x][$y]==255 )
              { $c="#"; $colour=2; }

            # Anti-aliased shadow (same as shadow)
            if($blue[$x][$y]<255 && $red[$x][$y]==0)
              { $c="."; $colour=3; }

            # Solid shadow
            if($blue[$x][$y]==0 && $red[$x][$y]==0)
              { $c=" "; $colour=3; }

            print $c;

            if($colour != $lastcolour)
              {
                if($lastcolour!=-1)
                  { $linecode.=" $lastcolour,$count";
                    $bytecode.=pack("C2",$lastcolour,$count);
                  }
                $lastcolour=$colour; $count=1;
              }
            else
              { $count++; }
          }
        if($lastcolour!=-1)
          { $linecode.=" $lastcolour,$count";
            $bytecode.=pack("C2",$lastcolour,$count);
          }
         print " [$linecode]\n";

        # Put length of RLE line
        print OUTPUT pack("C*",length($bytecode)/2);

        # Put RLE line
        print OUTPUT $bytecode;

      }

    print "\n";

    $xstart=$xend+1;
    $char++;
  }

print OUTPUT pack("C",255);

print "Done!\n";

close(OUTPUT);
