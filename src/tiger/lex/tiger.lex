%filenames = "scanner"

 /*
  * Please don't modify the lines above.
  */

 /* You can add lex definitions here. */
 /* TODO: Put your lab2 code here */

%x COMMENT STR

%%

/*
  * Below is examples, which you can wipe out
  * and write regular expressions and actions of your own.
  *
  * All the tokens:
  *   Parser::ID
  *   Parser::STRING
  *   Parser::INT
  *   Parser::COMMA
  *   Parser::COLON
  *   Parser::SEMICOLON
  *   Parser::LPAREN
  *   Parser::RPAREN
  *   Parser::LBRACK
  *   Parser::RBRACK
  *   Parser::LBRACE
  *   Parser::RBRACE
  *   Parser::DOT
  *   Parser::PLUS
  *   Parser::MINUS
  *   Parser::TIMES
  *   Parser::DIVIDE
  *   Parser::EQ
  *   Parser::NEQ
  *   Parser::LT
  *   Parser::LE
  *   Parser::GT
  *   Parser::GE
  *   Parser::AND
  *   Parser::OR
  *   Parser::ASSIGN
  *   Parser::ARRAY
  *   Parser::IF
  *   Parser::THEN
  *   Parser::ELSE
  *   Parser::WHILE
  *   Parser::FOR
  *   Parser::TO
  *   Parser::DO
  *   Parser::LET
  *   Parser::IN
  *   Parser::END
  *   Parser::OF
  *   Parser::BREAK
  *   Parser::NIL
  *   Parser::FUNCTION
  *   Parser::VAR
  *   Parser::TYPE
  */

 /* reserved words */
"array" {adjust(); return Parser::ARRAY;}
"if" {adjust(); return Parser::IF;}
"then" {adjust(); return Parser::THEN;}
"else" {adjust(); return Parser::ELSE;}
"while" {adjust(); return Parser::WHILE;}
"for" {adjust(); return Parser::FOR;}
"end" {adjust(); return Parser::END;}
"break" {adjust(); return Parser::BREAK;}
"function" {adjust(); return Parser::FUNCTION;}
"var" {adjust(); return Parser::VAR;}
"type" {adjust(); return Parser::TYPE;}
"in" {adjust(); return Parser::IN;}
"let" {adjust(); return Parser::LET;}
"to" {adjust(); return Parser::TO;}
"do" {adjust(); return Parser::DO;}
"of" {adjust(); return Parser::OF;}
"nil" {adjust(); return Parser::NIL;}

"&" {adjust(); return Parser::AND;}
"|" {adjust(); return Parser::OR;}

"+" {adjust(); return Parser::PLUS;}
"-" {adjust(); return Parser::MINUS;}
"*" {adjust(); return Parser::TIMES;}
"/" {adjust(); return Parser::DIVIDE;}
":=" {adjust(); return Parser::ASSIGN;}

"=" {adjust(); return Parser::EQ;}
"<>" {adjust(); return Parser::NEQ;}
"<" {adjust(); return Parser::LT;}
"<=" {adjust(); return Parser::LE;}
">" {adjust(); return Parser::GT;}
">=" {adjust(); return Parser::GE;}

"(" {adjust(); return Parser::LPAREN;}
")" {adjust(); return Parser::RPAREN;}
"[" {adjust(); return Parser::LBRACK;}
"]" {adjust(); return Parser::RBRACK;}
"{" {adjust(); return Parser::LBRACE;}
"}" {adjust(); return Parser::RBRACE;}

";" {adjust(); return Parser::SEMICOLON;}
":" {adjust(); return Parser::COLON;}
"," {adjust(); return Parser::COMMA;}
"." {adjust(); return Parser::DOT;}

[[:alpha:]_][[:alnum:]_]* {adjust();string_buf_=matched();return Parser::ID;}
[[:digit:]]+ {adjust();string_buf_=matched();return Parser::INT;}

\" {adjust();begin(StartCondition__::STR);string_buf_.clear(); }
"/*" {adjust();comment_level_=0;begin(StartCondition__::COMMENT);}

<STR>{
  \" {adjustStr();setMatched(string_buf_);begin(StartCondition__::INITIAL);return Parser::STRING;}
  \\n {adjustStr();string_buf_+='\n';}
  \\t {adjustStr();string_buf_+='\t';}
  \\\" {adjustStr();string_buf_+='\"';}
  \\\\ {adjustStr();string_buf_+='\\';}
  \\[[:digit:]]{3} {adjustStr();string_buf_+=(char)atoi(matched().c_str()+1);}
  \\\^[A-Z] {adjustStr();string_buf_+=matched()[2]-'A'+1;}
  \\[ \t\n\f]+\\ {adjustStr();}
  . {adjustStr();string_buf_+=matched();}
}



<COMMENT>{
  "*/" {adjustStr(); if(comment_level_)comment_level_--;else begin(StartCondition__::INITIAL);}
  "/*" {adjustStr(); comment_level_++;}
  \n|. {adjustStr();}
}


 /*
  * skip white space chars.
  * space, tabs and LF
  */
[ \t]+ {adjust();}
\n {adjust(); errormsg_->Newline();}

 /* illegal input */
. {adjust(); errormsg_->Error(errormsg_->tok_pos_, "illegal token");}


