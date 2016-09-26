## Table of Contents

- [Introduction](#intro)
- [Terminologies](#term)
- [GFA: Mandatory Fields](#gfafmt)
  - [Segment line](#sline)
  - [Link line](#lline)
  - [Gap line](#gline)
  - [Match line](#mline)
- [TODO](#todo)

## <a name="intro"></a>Introduction

GFA stands for Graphical Fragment Assembly format. It is a TAB-delimited text
format to describe the relationships between sequences. Initially designed for
[sequence assembly][sa], GFA may also represent variations in genomes and
splice graphs in genes.

For an example, in the picture below, each line is a nucleotide sequence with
the arrow indicating its orientation:

![image](https://cloud.githubusercontent.com/assets/480346/18406447/0c612dd6-76cb-11e6-83e5-a7ffdb2d33c0.png)

Assuming each sequence is 200bp in length and each overlap is an exact match,
we can encode this graph in the GFA format as:
```
S  A  *	LN:i:200
S  B  *	LN:i:200
S  C  *	LN:i:200
S  D  *	LN:i:200
L  A  +  B  +  40M
L  A  +  C  -  20M
L  B  +  D  +  20M
L  C  -  D  +  40M
```
where each S-line, or *segment* line, gives the property of a sequence,
including its length and actual nucleotide sequence; each L-line, or *link*
line, describes the relationship between two segments. There are two common
ways to understand an L-line. We take `L A + C -` as an example. First, in the
overlap graph view, the L-line indicates sequence *A* on the forward strand is
ahead of *C* on the reverse strand. Second, in the [string graph][sg] view, the
end of *A* transits to the start of *C* (i.e. `+` for the end of a sequence and
`-` for the start). In the following, we often take the overlap graph view for
convenience.

Notably, if `L A + C -` is a link, `L C + A -` is also a link because the two
are equivalent:

![image](https://cloud.githubusercontent.com/assets/480346/18406941/97fe1bc2-76d2-11e6-8a7e-3011db7c6ca8.png)

## <a name="term"></a>Terminologies

* **Segment**: a sequence. An **oriented segment** is a 2-tuple
  (segment,strand). Each oriented segment has a *complement* oriented segment
  (segment,¬strand), where operator `¬` gives the opposite strand.

* **Link**: a (full) dovetail overlap between two oriented segments. A link
  is directed. Each link has a *complement* link derived by swapping the order
  of segments and flipping the orientations. For the definition of *dovetail
  overlap*, see documentations [from GRC][grcdef] or [from
  wgs-assembler][caovlp].

Mathematically, GFA models a [skew-symmetric graph][ssg], where each vertex is
an oriented segment (in the overlap graph view) or the 5'- or 3'-end of a
segment (in the string graph view), and each directed edge is a link in GFA.

## <a name="gfafmt"></a>GFA: Mandatory Fields

In GFA, each line is TAB-delimited and describes only one type of data. On each
line, the leading letter indicates the data type and defines the mandatory
fields on that line. The following table gives an overview of different line
types in GFA:

|Line|Type       |col1|col2|col3|col4|col5   |
|:--:|:----------|:---|:---|:---|:---|:------|
|`H` |Header     ||||||||
|`S` |Segment    |sid |seq ||||||
|`L` |Link       |sid1|ori1|sid2|ori2|overlap|
|`C` |Containment|sid1|sid2|ori |CIGAR||
|`#` |Comment    ||||||||

### <a name="sline"></a> Segment line

A segment line or S-line takes the following format:
```
S	<sid>	<seq>
```
where *sid* is the segment name and *seq* is the sequence which can be `*` if
not available.

### <a name="lline"></a> Link line

A link line or L-line is
```
L	<sid1>	<ori1>	<sid2>	<ori2>	<overlap>
```
where *sid1*/*sid2* are the names of segments involved in the link,
*ori1*/*ori2* are orientations (either `+` or `-`), and *overlap* may be a
CIGAR `/^([0-9]+S)?([0-9]+[MID])([0-9]+N)?$/`.


*olen1*/*olen2* are the
lengths in the overlap as is shown in the following (o1 and o2 in the figure
correspond to *olen1* and *olen2*, respectively):

![image](https://cloud.githubusercontent.com/assets/480346/18406012/93192960-76c5-11e6-9d8d-f0f68c1199d4.png)

In GFA, each L-line has a complement L-line, which is
```
L	<sid2>	¬<ori2>	<sid1>	¬<ori1>	<olen2>	<olen1>
```

### <a name="gline"></a> Gap line

A gap line or G-line is
```
G	<sid1>	<ori1>	<sid2>	<ori2>	<dist>
```
where *dist* is the best estimate between the ends of two segments. A G-line is
effectively an L-line with negative overlap length. 

### <a name="mline"></a> Match line

A match line or M-line is
```
M	<sid1>	<ori>	<sid2>	<beg1>	<end1>	<beg2>	<end2>
```
where *sid1*/*sid2* are the names of segments, [*beg1*,*end1*) gives the
interval on the forward strand of *sid1* and [*beg2*,*end2*) gives the interval
on the *ori* strand of *sid2*.

Each M-line also has a complement M-line.

## <a name="todo"></a> TODO

1. Optional fields
2. Paths and Threads (for traversing matches)
3. To store both link and complement link or not
4. Nail down the format of M-line (alternative: Jason's way or the PAF/BLAST way)

[sa]: https://en.wikipedia.org/wiki/Sequence_assembly
[sg]: http://bioinformatics.oxfordjournals.org/content/21/suppl_2/ii79.abstract
[ssg]: https://en.wikipedia.org/wiki/Skew-symmetric_graph
[grcdef]: http://www.ncbi.nlm.nih.gov/projects/genome/assembly/grc/info/definitions.shtml
[caovlp]: http://wgs-assembler.sourceforge.net/wiki/index.php/Overlaps
