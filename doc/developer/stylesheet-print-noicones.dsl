<!DOCTYPE style-sheet PUBLIC "-//James Clark//DTD DSSSL Style Sheet//EN" [
<!ENTITY dbstyle SYSTEM
"/usr/share/sgml/docbook/stylesheet/dsssl/modular/print/docbook.dsl" CDATA DSSSL>
]>

<style-sheet>
<style-specification use="docbook">
<style-specification-body>

; Personnalization of James Clark's stylesheet for PS & PDF output
; These parameters overide James Clark's one.
; Written by Alexis de Lattre (alexis@videolan.org)

; Only produce a table of contents (not a table of figure, etc...)
(define ($generate-book-lot-list$)
  (list ))

; Depth of the table of contents
(define (toc-depth nd)
      2)

; Magins
(define %left-margin%
  3pi)

(define %right-margin%
  3pi)

(define %top-margin%
  5pi)

(define %bottom-margin%
  3.5pi)

(define %header-margin%
  2pi)

(define %footer-margin%
  2pi)

</style-specification-body>
</style-specification>
<external-specification id="docbook" document="dbstyle">
</style-sheet>
