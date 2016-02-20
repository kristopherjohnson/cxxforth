80 constant /buf
create buf  /buf chars allot

: test-accept ( -- )
    begin
        ." Enter a line (empty to quit) >" cr
        buf /buf accept
    dup while
        cr ." Line length: " dup .
        cr ." Line content: |" buf swap type ." |"
        cr
    repeat
    drop
;

