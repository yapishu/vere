/-  *modulo
/+  *server
/=  index
  /^  octs
  /;  as-octs:mimes:html
  /:  /===/app/modulo/index
  /|  /html/
      /~  ~
  ==
/=  modulo-js
  /^  octs
  /;  as-octs:mimes:html
  /:  /===/app/modulo/script
  /|  /js/
      /~  ~
  ==
=,  format
|%
:: +move: output effect
::
+$  move  [bone card]
:: +card: output effect payload
::
+$  card
  $%  [%connect wire binding:http-server term]
      [%serve wire binding:http-server generator:http-server]
      [%disconnect wire binding:http-server]
      [%http-response =http-event:http]
      [%poke wire dock poke]
      [%diff %json json]
  ==

+$  poke
  $%  [%modulo-bind app=term]
      [%modulo-unbind app=term]
  ==
::
+$  state
  $%  $:  %0   
          session=(map term @t)
          order=(list term)
          cur=(unit [term @])
      ==
  ==
::
++  session-as-json
  |=  [cur=(unit [term @]) session=(map term @t) order=(list term)]
  ^-  json
  ?~  cur
    *json
  %-  pairs:enjs
  :~  [%app %s -.u.cur]
      [%url %s (~(got by session) -.u.cur)]
      :-  %list
        :-  %a
      %+  turn  order
        |=  [a=term]
        [%s a]
  ==
::
--
::
|_  [bow=bowl:gall sta=state]
::
++  this  .
::
++  prep
  |=  old=(unit *)
  ^-  (quip move _this)
  ?~  old
    :_  this
    [ost.bow %connect / [~ /] %modulo]~
  [~ this]
::
::  alerts us that we were bound. we need this because the vane calls back.
::
++  bound
  |=  [wir=wire success=? binding=binding:http-server]
  ^-  (quip move _this)
  [~ this]
::
++  peer-applist
  |=  [pax=path]
  ^-  (quip move _this)
  :_  this
  [ost.bow %diff %json (session-as-json cur.sta session.sta order.sta)]~
::
++  session-js
  ^-  octs
::  ?~  cur.sta
::    *octs
  %-  as-octt:mimes:html
  ;:  weld
::      (trip 'window.onload = function() {')
      "window.ship = '{+:(scow %p our.bow)}';"
      "window.urb = new Channel();"
  ==
::      (trip '};')
::  ==
::      "  window.state = "
::      (en-json:html (session-as-json cur.sta session.sta order.sta))
::      (trip '}();')
::      %-  trip
::      '''
::      document.onkeydown = (event) => {
::        if (!event.metaKey || event.keyCode !== 75) { return; }
::        window.parent.postMessage("commandPalette", "*");
::      };
::      '''
::
::  +poke-handle-http-request: received on a new connection established
::
++  poke-handle-http-request
  %-  (require-authorization:app ost.bow move this)
  |=  =inbound-request:http-server
  ^-  (quip move _this)
  ::
  =/  request-line  (parse-request-line url.request.inbound-request)
  =/  site  (flop site.request-line)
  ?~  site
    [[ost.bow %http-response (redirect:app '~landscape')]~ this]
  ?+  site
    [[ost.bow %http-response (html-response:app index)]~ this]
      [%session *]
    [[ost.bow %http-response (js-response:app session-js)]~ this]
      [%script *]
    [[ost.bow %http-response (js-response:app modulo-js)]~ this]
  ==
::  +poke-handle-http-cancel: received when a connection was killed
::
++  poke-handle-http-cancel
  |=  =inbound-request:http-server
  ^-  (quip move _this)
  ::  the only long lived connections we keep state about are the stream ones.
  ::
  [~ this]
::
++  poke-modulo-bind
  |=  bin=term
  ^-  (quip move _this)
  =/  url  (crip "~{(scow %tas bin)}")
  ?:  (~(has by session.sta) bin)
    [~ this]
  :-  [`move`[ost.bow %connect / [~ /[url]] bin] ~]
  %=  this
      session.sta
    (~(put by session.sta) bin url)
  ::
      order.sta
    (weld order.sta ~[bin])
  ::
      cur.sta
    ?~  cur.sta  `[bin 0]
      cur.sta
  ==
::
++  poke-modulo-unbind
  |=  bin=term
  ^-  (quip move _this)
  =/  url  (crip "~{(scow %tas bin)}")
  ?.  (~(has by session.sta) bin)
    [~ this]
  =/  ind  (need (find ~[bin] order.sta))
  =/  neworder  (oust [ind 1] order.sta)
  :-  [`move`[ost.bow %disconnect / [~ /(crip "~{(scow %tas bin)}")]] ~]
  %=  this
    session.sta  (~(del by session.sta) bin)
    order.sta    neworder
    cur.sta
    ::
    ?:  =(1 (lent order.sta))
      ~
    ?:  (lth ind +:(need cur.sta))
      `[-:(need cur.sta) (dec +:(need cur.sta))]
    ?:  =(ind +:(need cur.sta))
      `[(snag 0 neworder) 0]
    cur.sta
  ==
::
++  poke-modulo-command
  |=  com=command
  ^-  (quip move _this)
  =/  length  (lent order.sta)
  ?~  cur.sta
    [~ this]
  ?:  =(length 1)
    [~ this]
  =/  new-cur=(unit [term @])
  ?-  -.com
    %forward
      ?:  =((dec length) +.u.cur.sta)
        `[(snag 0 order.sta) 0]
      =/  ind  +(+.u.cur.sta) 
      `[(snag ind order.sta) ind]
    %back
      ?:  =(0 +.u.cur.sta)
        =/  ind  (dec length)
        `[(snag ind order.sta) ind]
      =/  ind  (dec +.u.cur.sta)
      `[(snag ind order.sta) ind]
    %go
      =/  ind  (find [app.com]~ order.sta)
      ?~  ind
        cur.sta
      `[app.com u.ind]
  ==
  :_  this(cur.sta new-cur)
  %+  turn  (prey:pubsub:userlib /applist bow)
  |=  [=bone ^]
  [bone %diff %json (session-as-json new-cur session.sta order.sta)]
::
--
