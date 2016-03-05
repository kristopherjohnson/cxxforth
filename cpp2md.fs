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

: 3drop   drop 2drop ;

\ Determine whether two addresses point to different characters.
: mismatch? ( caddr1 caddr2 -- caddr1 caddr2 flag )
    2dup c@ swap c@ <>
;

\ Determine whether two strings start with same characters.
: same-prefix? ( caddr1 u1 caddr2 u2 -- flag )
    >r swap r> min             ( caddr1 caddr2 len )

    \ If either string is empty, return false.
    dup 0= if
        3drop
        false exit
    then

    begin
        dup 0>
    while
        >r                     ( caddr1 caddr2 ) ( R: len )
        mismatch? if
            2drop r> drop
            false exit
        then
        char+ swap char+
        r> 1-                  ( caddr2+1 caddr1+1 len-1 )
    repeat
    3drop
    true
;

: comment-start? ( caddr u -- flag ) s" /****" same-prefix? ;
: comment-end?   ( caddr u -- flag ) s" ****/" same-prefix? ;

: check-input  ( ior -- ) abort" Unable to open input file" ;
: check-output ( ior -- ) abort" Unable to open output file" ;
: check-read   ( ior -- ) abort" Read error" ;
: check-write  ( ior -- ) abort" Write error" ;

variable inFile         \ input file ID
variable outFile        \ output file ID
variable inCodeSection? \ flag: Are we in a C++ section?

\ Buffer for get-input-line.
\ Actual size must be 2 characters larger than the usable size.
510 constant #lineBuf
create lineBuf  #lineBuf 2 +  chars allot

\ Read next input line.
\ Return ( caddr length true ) on success.
\ Return ( caddr 0 false ) at end-of-file
: get-input-line ( -- caddr length flag )
    lineBuf dup #lineBuf inFile @ read-line check-read
;

\ Perform the conversion described above.
\ inFile and outFile must already contain the file IDs of
\ the input file and output file.
: convert-to-markdown ( -- )
    true inCodeSection? !
    begin
        get-input-line
    while
        2dup comment-start? if
            false inCodeSection? !
        else
            2dup comment-end? if 
                true inCodeSection? !
            else
                inCodeSection? @ if
                    s"     " outFile @ write-file check-write
                then
                2dup outFile @ write-line check-write
            then
        then
        2drop
    repeat
    2drop
;

\ Main entry point
: go ( -- )
    s" cxxforth.cpp" r/o open-file check-input inFile !
    s" cxxforth.cpp.md" r/w create-file check-output outFile !

    convert-to-markdown

    inFile @ close-file drop
    outFile @ close-file drop
;

go
bye

