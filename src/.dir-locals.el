; see also https://github.com/erdc-cm/petsc-dev/blob/master/.dir-locals.el
(
 (nil . ((indent-tabs-mode . nil)
         (tab-width . 4)
         (fill-column . 80)))
 ;; Warn about spaces used for indentation:
 (c-mode . ((c-file-style . "bsd")
	    (c-basic-offset . 4)
	    (c-comment-only-line-offset . 4)
	    ))
 (haskell-mode . ((eval . (highlight-regexp "^ *"))))
 (java-mode . ((c-file-style . "bsd"))))
