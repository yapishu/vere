::
::::  hoon/marc/gen
  ::
:-  %say
|=  *
:-  %noun
=>  |%
    ++  item  (pair mite (list flow))                   ::  xml node generator
    ++  colm  @ud                                       ::  column
    ++  flow  (each manx twig)                          ::  node or generator
    ++  form  (unit $?($emph $bold $code))              ::  formatting
    ++  mite                                            ::  context
      $?  $down                                         ::  outer embed
          $list                                         ::  unordered list
          $lime                                         ::  list item
          $lord                                         ::  ordered list
          $poem                                         ::  verse
          $bloc                                         ::  blockquote
          $code                                         ::  preformatted code
          $expr                                         ::  dynamic expression
      ==
    ++  trig                                            ::  line style
      $:  col/@ud                                       ::  start column 
          $=  sty                                       ::  style
          $?  $done                                     ::   terminator
              $none                                     ::  end of input
              $lint                                     ::  + line item
              $lite                                     ::  - line item
              $head                                     ::  # heading
              $text                                     ::  anything else
      ==  ==                                            ::
    ++  word  @t                                        ::  source text
    --
|%                                                      ::  
++  cram                                                ::  markdown with errors
  |=  {naz/hair los/tape}
  ^-  (like item)
  ::
  ::  err: error position
  ::  col: current control column
  ::  hac: stack of items under construction
  ::  cur: current item under construction
  ::  lub: current block being read in
  ::
  =|  err/(unit hair)
  =/  col  q.naz
  =|  hac/(list item)
  =/  cur/item  [%down ~]
  =|  lub/(unit (pair hair (list tape)))
  =<  $:line
  |%
  ::                                                    ::
  ++  $                                                 ::  resolve
    ^-  (like item)
    ::  if error position is set, produce error
    ::
    ?.  =(~ err)  [+.err ~]
    ::  all data was consumed
    ::
    =-  [naz `[- [naz los]]]
    |-  ^-  item
    ::  fold all the way to top
    ::
    ?~  hac  cur
    $(..^$ fold)
  ::                                                    ::
  ++  back                                              ::  column retreat
    |=  luc/@ud
    ^+  +>
    ?:  =(luc col)  +>
    ::  nex: next backward step that terminates this context
    ::
    =/  nex/@ud
      ?-  p.cur
        $down  0
        $expr  2
        $list  0
        $lime  2
        $lord  0
        $poem  8
        $code  4
        $bloc  6
      ==
    ?:  (gth nex (sub col luc))
      ::  indenting pattern violation
      ::
      ..^$(err `[p.naz luc])
    $(..^$ fold, col (sub col nex))
  ::                                                    ::
  ++  fine                                              ::  item to flow
    ^-  flow
    =-  [%& [[- ~] q.cur]]
    ::  only items which contain flows
    ::
    ?+  p.cur  !!
      $list  %ul
      $lord  %ol
      $lime  %li
      $bloc  %bq
      $code  %pre
    ==
  ::                                                    ::
  ++  fold  ^+  .                                       ::  complete and pop
    ?~  hac  .
    %=  .
      hac  t.hac
      cur  [p.i.hac [fine q.i.hac]]
    ==
  ::                                                    ::
  ++  snap                                              ::  capture raw line
    =|  nap/tape
    |-  ^+  [nap +>]
    ::  no unterminated lines
    ::
    ?~  los  [~ +>(err `naz)]
    ?:  =(`@`10 i.los)  
      ::  consume newline
      ::
      :_  +>(los t.los, naz [+(p.naz) 1])
      ::  trim trailing spaces
      ::
      |-  ^-  tape 
      ?:  ?=({$' ' *} nap) 
        $(nap t.nap) 
      (flop nap)
    $(los t.los, q.naz +(q.naz), nap [i.los nap])
  ::                                                    ::
  ++  skip  +:snap                                      ::  discard line
  ++  look                                              ::  inspect line
    ^-  (unit trig)
    ?~  los
      `[q.naz %none]
    ?:  =(`@`10 i.los)
      ~
    ?:  =(' ' i.los)
      look(los t.los, q.naz +(q.naz))
    :-  q.naz
    ?:  =('\\' i.los)
      %done
    ?:  =('\\' i.los)
      %head
    ?:  ?=({$'-' $' ' *} los)
      %lite
    ?:  ?=({$'-' $' ' *} los)
      %lint
    %text
  ::                                                    ::
  ++  cape                                              ::  xml-escape
    |=  tex/tape
    ^-  tape
    ?~  tex  tex
    =+  $(tex t.tex)
    ?+  i.tex  [i.tex -]
      $34  ['&' 'q' 'u' 'o' 't' ';' -]
      $38  ['&' 'a' 'm' 'p' ';' -]
      $39  ['&' '#' '3' '9' ';' -]
      $60  ['&' 'l' 't' ';' -]
      $62  ['&' 'g' 't' ';' -]
    ==
  ::                                                    ::
  ++  made                                              ::  compose block
    ^+  . 
    ::  empty block, no action
    ::
    ?~  lub  .
    ::  if block is preformatted code
    ::
    ?:  ?=($code p.cur)
      ::  add blank line between blocks
      ::
      =.  q.cur
        ?~  q.cur  q.cur
        :_(q.cur `manx`[[%$ [%$ `tape`[`@`10 ~]]] ~])
      %=    .
          q.cur  
        %+  weld
          %+  turn
            q.u.lub
          |=  tape  ^-  flow
          ::  each line is text data with its newline
          ::
          :-  %&
          [[%$ [%$ (weld (slag col +<) `tape`[`@`10 ~]]] ~])
        q.cur
      ==
    ::  if block is verse
    ::
    ?:  ?=($poem p.cur)
      ::  add break between stanzas
      ::
      =.  q.cur  ?~(q.cur q.cur [[[%br ~] ~] q.cur])
      %=    .
          q.cur  
        %+  weld
          %+  turn
            q.u.lub
          |=  tape  ^-  flow
          ::  each line is a paragraph
          ::
          :-  %&
          :-  [%p ~]
          :_  ~
          [[%$ [%$ (weld (slag col +<) `tape`[`@`10 ~]]] ~])
        q.cur
      ==
    ::  yex: block recomposed for reparsing
    ::
    =/  yex/tape  (zing (flop q.u.lub))
    ::  if block is dynamic
    ::
    ?:  ?=($expr p.cur)
      ::  sab: rule for embedded twig
      ::
      =*  sab  (ifix [gay ;~(plug (star ace) (just `@`10))] tall:vast)
      ::  vex: product of parsing following twig
      ::
      =/  vex/(like twig)  (sab naz los)
      ::  fail upward if parse failed
      ::
      ?~  q.vex  ..^$(err `p.vex)
      ::  otherwise, add item and continue
::    ::
::    %=  $
::      naz  p.q.u.vex
::      los  q.q.u.vex
::      lap  :_(lap [%| %marl p.u.vex])
::    ==
  ::                                                    ::  
  ++  line  ^+  .                                       ::  body line loop
    ::  abort after first error
    ::
    ?:  !=(~ err)  .
    ::  pic: profile of this line
    ::
    =/  pic  look
    ::  if line is blank
    ::
    ?~  pic
      ::  break section
      ::
      line:made:skip
    ::  line is not blank
    ::
    =>  .(pic u.pic)
    ::  if end of input, complete
    ::
    ?:  ?=($none sty.pic)
      ..$(q.naz col.pic)
    ::  if end marker behind current column
    ::
    ?:  &(?=($done sty.pic) (lth col.pic col))
      ::  retract and complete
      ::
      (back(q.naz (add 2 col.pic)) col.pic)
    ::  bal: inspection copy of lub, current section
    ::
    =/  bal  lub
    ::  if within section
    ::
    ?^  bal
      ::  detect bad block structure
      ::
      ?:  ?|  ?=($head sty.pic)
              ?:  ?=(?($code $poem $expr) p.cur)
                (lth col.pic col)
              |(!=(%text sty.pic) !=(col.pic col))
          ==
        ..$(err `[p.naz col.pic])
      ::  accept line and continue
      :: 
      =^  nap  ..$  snap
      line(lub bal(q.u [nap q.u.bal]))
    ::  if column has retreated, adjust stack
    ::
    =.  ..$  ?:  (lth col.pic col)  ..$
        (back col.pic)
    ::  dif: columns advanced
    ::  erp: error position
    ::
    =/  dif  (sub col.pic col)
    =/  erp  [p.naz col.pic]
    =.  col  col.pic
    ::  nap: take first line
    ::
    =^  nap  ..$  snap
    ::  execute appropriate paragraph form
    ::
    =<  line:abet:apex
    |%
    ::                                                  ::  
    ++  abet                                            ::  accept line
      ..$(lub `[naz nap ~])
    ::                                                  ::
    ++  apex  ^+  .                                     ::  by column offset
      ?+  dif  fail
        $0  apse
        $2  expr
        $4  code
        $6  bloc
        $8  poem
      ==
    ::                                                  ::
    ++  apse  ^+  .                                     ::  by prefix style
      ?-  sty.pic
        $done  !!
        $head  head
        $lite  lite
        $lint  lint
        $text  text
      ==
    ::                                                  ::
    ++  bloc  apse:(push %bloc)                         ::  blockquote line
    ++  fail  .(err `erp)                               ::  set error position
    ++  push  |=(mite %_(+> hac [cur hac], cur [+< ~])) ::  push context
    ++  expr  (push %expr)                              ::  hoon expression
    ++  code  (push %code)                              ::  code literal
    ++  poem  (push %poem)                              ::  verse literal
    ++  head  !!
    ++  lent                                            ::  list entry
      |=  ord/?
      ^+  +>
      ::  erase list marker
      ::
      =.  nap  =+(+(col) (runt [- ' '] (slag - nap)))
      ::  indent by 2
      ::
      =.  col  (add 2 col)
      ::  can't switch list types
      ::
      ?:  =(?:(ord %list %lord) p.cur)  fail
      ::  push list item
      ::
      %.  %lime
      =<  push
      ::  push list context, unless we're in list
      ::
      =+  ?:(ord %lord %list)
      ?:  =(- p.cur)  ..push  (push -)
    ::
    ++  lint  (lent &)                                  ::  numbered list
    ++  lite  (lent |)                                  ::  unnumbered list
    ++  text                                            ::  plain text
      ^+  .
      ::  except in lists, continue in current flow
      ::
      ?.  ?=(?($list $lord) p.cur)  .
      ::  in lists, complete current list and pop
      ::
      ?>  ?=(^ hac)
      %=  .
        hac  t.hac
        cur  [p.i.hac [[%& cur] q.i.hac]] 
      ==
    --
  --
--
