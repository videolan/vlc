<!DOCTYPE style-sheet PUBLIC "-//James Clark//DTD DSSSL Style Sheet//EN" [
<!ENTITY dbstyle SYSTEM "/usr/share/sgml/docbook/stylesheet/dsssl/modular/html/docbook.dsl" CDATA DSSSL>
]>

<style-sheet>
<style-specification use="docbook">
<style-specification-body>

; Personnalization of James Clark's stylesheet for HTML output
; These parameters overide James Clark's one.

; Change of HTML page for each chapter, not for each "sect1"
(define (chunk-element-list)
  (list ))

; "screens" should be in verbatim mode"
(define %shade-verbatim%
  #t)

; I want small icons for "notes", "warnings", "caution" & "important"
(define %admon-graphics%
  #t)
; Path for the icons
(define %admon-graphics-path%
  "../common/")

; Use ID attributes as name for component HTML files
(define %use-id-as-filename%
  #t)

; Are sections enumerated ?
(define %section-autolabel%
  #t)


</style-specification-body>
</style-specification>
<external-specification id="docbook" document="dbstyle">
</style-sheet>
