#!/usr/bin/wish

gets stdin size
scan $size "%i %i" x y
canvas .can -width [expr $x+16] -height [expr $y+16] -borderwidth 0 -highlightthickness 0 -bg white
pack .can

proc rect {x y w h n t c} {
	set x [expr $x+8]
	set y [expr $y+8]
	.can create rectangle $x $y [expr $x+$w] [expr $y+$h] -fill $c
	.can create text $x $y -anchor nw -text $n -fill red -font "arial 18 bold"
	.can create text $x [expr $y+$h] -anchor sw -text $t -font "courier 12"
}

while {-1 != [gets stdin line]} {
	eval $line
}

