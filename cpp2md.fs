\ cpp2md converts a C++ source file to a Markdown file.
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

: 3drop  drop 2drop ;

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

: read-error? ( ior -- ) abort" Read error" ;
: write-error? ( ior -- ) abort" Write error" ;

variable infile    \ input file ID
variable outfile   \ output file ID
variable incode?   \ flag: Are we in a C++ section?

\ Buffer for input-line.
\ Actual size needs to be 2 characters larger than the usable size.
256 constant #linebuf
create linebuf  #linebuf 2 +  chars allot

: input-line ( -- caddr length flag )
    linebuf #linebuf infile @ read-line read-error?
    >r >r linebuf r> r>
;

\ Perform the conversion described above.
\ infile and outfile must already contain the file IDs of
\ the input file and output file.
: convert-to-markdown ( -- )
    true incode? !
    begin
        input-line
    while
        2dup comment-start?
        if
            false incode? !
        else
            2dup comment-end?
            if 
                true incode? !
            else
                incode? @
                if
                    s"     " outfile @ write-file write-error?
                then
                2dup outfile @ write-line write-error?
            then
        then
        2drop
    repeat
    2drop
;

\ Main entry point
: go ( -- )
    s" cxxforth.cpp" r/o open-file abort" Unable to open input file"
    infile !

    s" cxxforth.cpp.md" r/w create-file abort" Unable to open output file"
    outfile !

    convert-to-markdown

    infile @ close-file drop
    outfile @ close-file drop
;

go
bye

