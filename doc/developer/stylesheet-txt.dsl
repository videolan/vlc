<!DOCTYPE style-sheet PUBLIC "-//James Clark//DTD DSSSL Style Sheet//EN" [
<!ENTITY dbstyle SYSTEM "/usr/share/sgml/docbook/stylesheet/dsssl/modular/html/docbook.dsl" CDATA DSSSL>
]>

<style-sheet>
<style-specification use="docbook">
<style-specification-body>

; Personnalization of James Clark's stylesheet for Text output
; These parameters overide James Clark's one.
; Written by Alexis de Lattre (alexis@videolan.org)

; Only produce a table of contents (not a table of figure, etc...)
(define ($generate-book-lot-list$)
  (list ))

; Depth of the table of contents
(define (toc-depth nd)
      1)

; No icons
(define %admon-graphics%
  #f)

; Are sections enumerated?
(define %section-autolabel%
  #t)


</style-specification-body>
</style-specification>
<external-specification id="docbook" document="dbstyle">
</style-sheet>
