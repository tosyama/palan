" Vim syntax file
" Language:	Palan
" Maintainer:	Toshinobu YAMAGUCHI
" Last Change:	2019 Jul 18

if exists('b:current_syntax')
	finish
endif

syn keyword     paDeclaration       type var const func syscall ccall
syn keyword     paConditional       if else
syn keyword     paRepeat            while
syn keyword     paStatement         return break continue

hi def link     paConditional       Conditional
hi def link     paDeclaration       Keyword
hi def link     paRepeat            Repeat
hi def link		paStatement			Statement

syn keyword     paSIntType			sbyte int16 int32 int64 
syn keyword     paUIntType          byte uint16 uint32 uint64 
syn keyword     paFloType			flo32 flo64 

hi def link     paSIntType			Type
hi def link     paUIntType			Type
hi def link     paFloType			Type

syn region      paComment           start="//" end="$" contains=@Spell
hi def link     paComment           Comment

" String
syn region      paString            start=+"+ skip=+\\\\\|\\"+ end=+"+
hi def link     paString            String

" Regions
syn region      paBlock             start="{" end="}" transparent fold
syn region      paParen             start='(' end=')' transparent

" Integers
syn match       paDecimalInt        "\<\d\+\([Ee]\d\+\)\?\>"
hi def link     paDecimalInt       	Number 

" Floating point
syn match       paImaginary         "\<\d\+\.\d*\([Ee][-+]\d\+\)\?i\>"
hi def link     paImaginary         Number

let b:current_syntax = 'pa'