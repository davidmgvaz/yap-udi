((nil . ((eval . 
               (setq default-directory (locate-dominating-file
                                        buffer-file-name ".dir-locals.el")
                     ))))
 (nil . ((compile-command . "cd arch && make install")))
 (nil . ((eval . (ecb-add-source-path default-directory "" "n"))))
 (nil . ((eval . (ecb-activate))))
)