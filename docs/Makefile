PDF=out/xyzTrick_Software_Manual_zh.pdf

.PHONY: all pdf clean

all: pdf

pdf: $(PDF)

$(PDF): xyzTrick_Software_Manual_zh.md pandoc.yaml preamble.tex build.sh
	./build.sh

clean:
	rm -f $(PDF)
