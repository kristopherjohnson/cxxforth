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

\ Determine whether two strings start with same characters.
: same-prefix? ( caddr1 u1 caddr2 u2 -- flag )
    >r swap r> min             ( caddr1 caddr2 u )

    dup 0= if
        3drop
        false exit
    then

    begin
        dup 0>
    while
        >r 2dup c@ swap c@     ( caddr1 caddr2 char2 char1 ) ( R: u )
        <> if
            2drop r> drop
            false exit
        then
        char+ swap char+ r> 1- ( caddr2+1 caddr1+1 u-1 )
    repeat
    3drop
    true
;

: comment-start? ( caddr u -- flag ) s" /****" same-prefix? ;
: comment-end?   ( caddr u -- flag ) s" ****/" same-prefix? ;

: abort-read-error  ( ior -- ) abort" Read error" ;
: abort-write-error ( ior -- ) abort" Write error" ;

variable inFile         \ input file ID
variable outFile        \ output file ID
variable inCodeSection? \ flag: Are we in a C++ section?

\ Buffer for get-input-line.
\ Actual size must be 2 characters larger than the usable size.
256 constant #lineBuf
create lineBuf  #lineBuf 2 +  chars allot

\ Read next input line.
\ Return ( caddr length true ) on success.
\ Return ( caddr 0 false ) at end-of-file
: get-input-line ( -- caddr length flag )
    lineBuf dup #lineBuf inFile @ read-line abort-read-error
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
                    s"     " outFile @ write-file abort-write-error
                then
                2dup outFile @ write-line abort-write-error
            then
        then
        2drop
    repeat
    2drop
;

\ Main entry point
: go ( -- )
    s" cxxforth.cpp" r/o open-file abort" Unable to open input file"
    inFile !

    s" cxxforth.cpp.md" r/w create-file abort" Unable to open output file"
    outFile !

    convert-to-markdown

    inFile @ close-file drop
    outFile @ close-file drop
;

go
bye

