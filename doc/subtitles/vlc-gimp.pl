#!/usr/bin/perl

use Gimp ":auto";
use Gimp::Fu;

sub vlc_subtitler_font
{

    $shadowcolour=[0,0,0];
    $textcolour=[255,255,255];
    $backcolour=[0,0,255];
    $font="-*-utopia-bold-r-*-*-21-*-*-*-*-*-*-*";
    $border=3;
    $alias=1;

    $text="_  ";
    for($i=33; $i<127; $i++)
      { $text.=chr($i); $text.="  "; }

    # Create a new image of an arbitrary size with 
    $img = gimp_image_new(100, 100, RGB);
    
    # Create a new layer for the background of arbitrary size, and
    # add it to the image
    my $background = gimp_layer_new($img, 100, 100,
				    RGB, "Background", 100,
				    NORMAL_MODE);
    gimp_image_add_layer($background, 1);

    # Choose color of text
    gimp_palette_set_foreground($textcolour);

    # Create the text layer. Using -1 as the drawable creates a new layer.
    my $text_layer = gimp_text_fontname($img, -1, 0, 0, $text,
					$border, $alias,
					xlfd_size($font), $font);

    # Get size of the text drawable and resize the image and the
    # background layer to this size.
    my($width, $height) = ($text_layer->width, $text_layer->height);
    gimp_image_resize($img, $width, $height, 0, 0);
    gimp_layer_resize($background, $width, $height, 0, 0);

    gimp_layer_delete($text_layer);

    # Fill the background layer now when it has the right size.
    gimp_palette_set_background($backcolour);
    gimp_edit_fill($background, BG_IMAGE_FILL);

    my $shadowlayer, $textlayer;

    # Plot eight shadow copies of the text
    gimp_palette_set_foreground($shadowcolour);
    $shadowlayer = gimp_text_fontname($img, -1, 0, 0, $text,
                                        $border, $alias,
                                        xlfd_size($font), $font);
    gimp_layer_translate($shadowlayer, -2, 0);
    gimp_image_flatten($img);

    gimp_palette_set_foreground($shadowcolour);
    $shadowlayer = gimp_text_fontname($img, -1, 0, 0, $text,
                                        $border, $alias,
                                        xlfd_size($font), $font);
    gimp_layer_translate($shadowlayer, 2, 0);
    gimp_image_flatten($img);

    gimp_palette_set_foreground($shadowcolour);
    $shadowlayer = gimp_text_fontname($img, -1, 0, 0, $text,
                                        $border, $alias,
                                        xlfd_size($font), $font);
    gimp_layer_translate($shadowlayer, 0, -2);
    gimp_image_flatten($img);

    gimp_palette_set_foreground($shadowcolour);
    $shadowlayer = gimp_text_fontname($img, -1, 0, 0, $text,
                                        $border, $alias,
                                        xlfd_size($font), $font);
    gimp_layer_translate($shadowlayer, 0, 2);
    gimp_image_flatten($img);

    gimp_palette_set_foreground($shadowcolour);
    $shadowlayer = gimp_text_fontname($img, -1, 0, 0, $text,
                                        $border, $alias,
                                        xlfd_size($font), $font);
    gimp_layer_translate($shadowlayer, -1, -1);
    gimp_image_flatten($img);

    gimp_palette_set_foreground($shadowcolour);
    $shadowlayer = gimp_text_fontname($img, -1, 0, 0, $text,
                                        $border, $alias,
                                        xlfd_size($font), $font);
    gimp_layer_translate($shadowlayer, -1, 1);
    gimp_image_flatten($img);

    gimp_palette_set_foreground($shadowcolour);
    $shadowlayer = gimp_text_fontname($img, -1, 0, 0, $text,
                                        $border, $alias,
                                        xlfd_size($font), $font);
    gimp_layer_translate($shadowlayer, 1, -1);
    gimp_image_flatten($img);

    gimp_palette_set_foreground($shadowcolour);
    $shadowlayer = gimp_text_fontname($img, -1, 0, 0, $text,
                                        $border, $alias,
                                        xlfd_size($font), $font);
    gimp_layer_translate($shadowlayer, 1, 1);
    gimp_image_flatten($img);

    # Plot the text itself
    gimp_palette_set_foreground($textcolour);
    $textlayer = gimp_text_fontname($img, -1, 0, 0, $text,
                                        $border, $alias,
                                        xlfd_size($font), $font);
    gimp_image_flatten($img);

    return $img;
}

# register the script
register "vlc_subtitler_font",
    "vlc subtitler font",
    "vlc subtitler font",
    "Andrew Flintham",
    "Andrew Flintham",
    "2002-06-18",
    "<Toolbox>/Xtns/Perl-Fu/VLC Subtitles Font",
    "*",
    [],
    \&vlc_subtitler_font;

# Handle over control to gimp
exit main();
