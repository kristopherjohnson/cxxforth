\ cpp2md.fs converts a C++ source file to a Markdown file.
\
\ The rules it follows to do this are simple:
\
\ - Text contained in lines between `/****` and `****/` are Markdown-format
\   comments, and should be passed through as-is to the Markdown output file.
\
\ - All other lines are C++, and should be indented four spaces so they are
\   treated as code blocks.
\
\ The `/****` and `****/` tokens must be at the initial position on the line.
\
\ Invoke this script like this in the cxxforth source directory:
\
\     build/cxxforth cpp2md.fs
\
\ This script is also compatible with GForth:
\
\     gforth cpp2md.fs

variable inFile  \ input file ID
variable outFile \ output file ID

: check-input  ( ior -- )  abort" Unable to open input file" ;
: check-read   ( ior -- )  abort" Read error" ;
: open-input   ( -- )      s" cxxforth.cpp" r/o open-file check-input  inFile ! ;
: close-input  ( -- )      inFile @  close-file drop ;

: check-output ( ior -- )  abort" Unable to open output file" ;
: check-write  ( ior -- )  abort" Write error" ;
: open-output  ( -- )      s" cxxforth.cpp.md" r/w create-file check-output  outFile ! ;
: close-output ( -- )      outFile @  close-file drop ;

: open-files   ( -- )      open-input open-output ;
: close-files  ( -- )      close-input close-output ;

\ Buffer for get-input-line.
\ Actual size must be 2 characters larger than the usable size.
510 constant #lineBuf
create lineBuf  #lineBuf 2 +  chars allot

\ Read next input line.
\ Return ( caddr length -1 ) on success.
\ Return ( 0 ) at end-of-file
: get-input-line ( -- 0 | caddr length -1 )
    lineBuf dup #lineBuf inFile @ read-line check-read
    dup 0= if nip nip then ;

variable inCodeSection? \ flag: Are we in a C++ section?

\ Determine whether two strings start with same characters.
\ If either string is empty, return false.
: same-prefix? ( caddr1 u1 caddr2 u2 -- flag )
    >r swap r> min              ( caddr1 caddr2 minLen )
    dup 0= if nip nip exit then
    tuck compare 0= ;

: comment-start? ( caddr u -- flag )  s" /****" same-prefix? ;
: comment-end?   ( caddr u -- flag )  s" ****/" same-prefix? ;

\ Convert a line.
\ If line starts with "/****" then set inCodeSection? false.
\ If line starts with "****/" then set inCodeSection? true.
\ Otherwise, output the line, indented if inCodeSection? is true.
: convert-line ( caddr length -- )
    2dup comment-start? if
        false inCodeSection? !
    else 2dup comment-end? if
        true inCodeSection? !
    else
        inCodeSection? @ if
            s"     " outFile @ write-file check-write
        then
        2dup outFile @ write-line check-write
    then then
    2drop ;

\ Convert all lines of the input file.
\ inFile and outFile must already contain the file IDs of
\ the input file and output file.
: convert-to-markdown ( -- )
    true inCodeSection? !
    begin get-input-line while convert-line repeat ;

\ Main entry point
: go ( -- )  open-files convert-to-markdown close-files ;

go
bye

