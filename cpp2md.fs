\ cpp2md is a simple utility that converts a C++ source file to a Markdown file.
\ 
\ The rules it follows to do this are simple:
\ 
\ - Text contained in lines between `/ ****` and `**** /` are Markdown-format
\   comments, and should be passed through as-is to the Markdown output file.
\ 
\ - All other lines are C++, and should be indented four spaces so they are
\   treated as code blocks.
\ 
\ The `/ ****` and `**** /` tokens must be at the initial position on the line.
\ 
\ Note: The comment markers above are displayed above with a space between the
\ `/` and `****`.  You must not include that space when writing comments, but we
\ have to do it here so that the C++ compiler won't think we are trying to nest
\ such comments within this comment block.
\ 
\ Invoke this script like this in the cxxforth source directory:
\ 
\     build/cxxforth cpp2md.fs

: inpath  ( -- c-addr u)  s" cxxforth.cpp" ;
: outpath ( -- c-addr u ) s" cxxforth.cpp.md" ;

variable infile    \ input file ID
variable outfile   \ output file ID
variable incode?   \ flag: Are we in a C++ section?

\ Buffer for read-line.
\ Actual size needs to be 2 characters larger than the usable size.
256 constant #linebuf
create linebuf  #linebuf 2 +  chars allot

\ Determine whether two strings start with same characters.
: same-prefix? ( c-addr1 u1 c-addr2 u2 -- flag )
    >r swap r> min       ( c-addr1 c-addr2 u )
    dup 0= if
        drop 2drop
        false exit
    then
    begin
        dup 0>
    while
        >r
        2dup c@ swap c@  ( c-addr1 c-addr2 char2 char1 ) ( R: u )
        <> if
            2drop r> drop
            false exit
        then
        char+ swap char+
        r> 1-
    repeat
    drop 2drop
    true ;

: read-error? ( ior -- ) abort" Read error" ;
: write-error? ( ior -- ) abort" Write error" ;

\ Perform the conversion described above.
\ infile and outfile must already contain the file IDs of
\ the input file and output file.
: convert-to-markdown ( -- )
    begin
        linebuf #linebuf infile @ read-line read-error?
    while
        dup linebuf swap
        s" /****" same-prefix?
        if
            false incode? !
        else
            dup linebuf swap
            s" ****/" same-prefix?
            if 
                true incode? !
            else
                incode? @
                if
                    s"     " outfile @ write-file write-error?
                then
                dup linebuf swap
                outfile @ write-line write-error?
            then
        then
        drop
    repeat
    drop ;

: go ( -- )
    inpath r/o open-file abort" Unable to open input file"
    infile !

    outpath r/w create-file abort" Unable to open output file"
    outfile !

    convert-to-markdown

    infile @ close-file drop
    outfile @ close-file drop ;

go
bye

