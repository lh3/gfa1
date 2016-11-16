## Getting Started
```sh
git clone https://github.com/lh3/gfaview
cd gfaview
make
wget https://github.com/lh3/gfaview/releases/download/data/miniasm-ecoli.gfa.gz
./gfaview -rtbom miniasm-ecoli.gfa.gz > transformed.gfa
```

## Introduction

This repo implements a command-line tool as well as a library in C that parses,
validates and transforms assembly graphs in a dialect of the GFA1 format, which
is loosely defined by the following grammar:
```
<id?>     ::= /[!-~]+/
<ori?>    ::= /[+-]/
<seq>     ::= /[!-~]+|\*/
<cigar>   ::= /([0-9]+[MIDSN])+/
<tag>     ::= /[A-Za-z][A-Za-z0-9_]:[ifAZB]:[ -~]+/
<olen?>   ::= /[0-9]+/

<format>  ::= ( <segment> | <link> )+
<segment> ::= S <id> <seq> <tag>*
<link>    ::= L <id1> <ori1> <id2> <ori2> <overlap> <tag>*
<overlap> ::= <olen> | <olen1>:<olen2> | <cigar>
```
The [gfa-chk.l][lex] file gives the more precise regular grammar that parses
the format. In comparison to the original GFA format, this dialect allows to
describe overlaps with one or two overlap lengths. For example, an overlap
`50:51` indicates a 50bp suffix of the first segment in the link overlaps with
a 51bp prefix of the second segment. An overlap `50` is a shorthand of `50:50`
or equivalently `50M`. The new overlap field is essential to long-read overlaps
as overlappers often do not output detailed CIGAR.

The command-line tool, gfaview, parses input graph and more importantly checks
its integrity. It discards segments with unknown lengths (e.g. segments used
on L-lines but not defined on S-lines), identifies duplicated links (when two
links have the same starting/ending vertices and the same overlap lengths),
and fills in missing information (e.g. for an overlap field like `50:` or
`:51`; missing S-lines but with inferrable segment lengths) as much as
possible. Gfaview outputs a correctly formatted GFA1 that is usually easier to
parse. Gfaview may also optionally apply a series of transformations such as
subsetting, tip trimming, bubble popping, short overlap purging and unitigging.
These operations are ported from miniasm and focus on long-read assembly for
now.

[lex]: https://github.com/lh3/gfa1/blob/master/gfa-chk.l
