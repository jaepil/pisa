# Efficient In-Memory Inverted Indexes

## Running PISA in container

TODO download instructions

You can use Docker or Podman to start the container. In the example
below, we mount a volume so that we can persist the files we create
after the container has terminated. We use a mounted volume to persist
the data on the local drive in an arbitrary directory
`$HOME/pisa-workdir` (can be any other directory on your local drive).

    mkdir -p "$HOME/pisa-workdir"
    docker run \
        --detach -it                        # run in background
        --network host \                    # to avoid connection issues
        --name=pisa \                       # name container
        -v "$HOME/pisa-workdir:/workdir" \  # mount volume
        pisa-tutorial                       # image name

We can now execute the container in an interactive terminal:

    docker exec -it pisa /bin/bash

### Note about RedHat based systems

If you are running a RedHat based system such as RHEL or Fedora, you may
need to run `sudo setenforce 0` for Podman.

## Section 1: Basic Usage

This section showcases the basic usage of PISA, including indexing a
collection, inspecting metadata, and querying the index. We will start
by indexing a toy collection using default parameters. We will then
explain the constructed artifacts and metadata. Once the index is built,
we will query the index.

Once we familiarize ourselves with the basic commands and concepts, we
will index a larger collection. We will take advantage of the
[`ir-datasets`](https://ir-datasets.com/) integration to obtain the
data. We will then explore querying the collection with multiple
retrieval algorithms and show the difference in performance. Finally, we
will build another index based on the same collection and compare size
and benchmarking results.

### Note on `pisa` command

Throughout this tutorial, we use `pisa` command for easy access to
indexing an querying capabilities of PISA. Note, however, this is an
experimental tool, written in Python, that calls various PISA commands.
It is subject to changes in the future. We will mention some of the
lower-level commands later. For those interested, you can find more
details about all PISA commands in
[our documentation](https://pisa-engine.github.io/pisa/book)'s CLI
Reference section.


### Toy Example

First, let's have a peek at our toy collection. Our file is in JSONL
format -- each line contains a JSON representing a document. We can use
the `jq` tool to pretty-print it:

    cat /data/tiny.jsonl | jq

Each document has a title and content, which is all we will need to
build an index. We can now pipe it to the `pisa` tool with the
appropriate parameters.

    pisa index stdin --format jsonl -o /workdir/toy < /data/tiny.jsonl

Let's break it down. We execute the `index` command, with `stdin`
subcommand, indicating that the collection will be read from the
standard input. We also specify `--format jsonl` in order to employ the
correct parser, as well as the output directory. Finally, we redirect
the content of our file to the program.

The collection is small, so it should finish almost immediately. We can
verify by listing the contents of the directory:

    ls /workdir/toy

We will get back to those later.

### WikIR collection

We can now proceed to indexing a real dataset. For convenience, we will
use the integration with [`ir-datasets`](https://ir-datasets.com/). All
we have to do is to provide the name of the dataset and which field(s) we
want to use as the content. We will use the English subset of the
[WikIR collection](https://ir-datasets.com/wikir.html).

    pisa index ir-datasets wikir/en1k \
        --content-fields text \
        -o /workdir/wikir

This time, it will take a while to index, as this collection contains
roughly 370,000 documents. The collection will also have to be
downloaded (~165 MiB) before the indexing starts. That said, the entire
command shouldn't run much longer than a couple of minutes.

Once it finishes, we can query the index. To avoid passing the index
location as a command argument, we can change directory to where the
index was built:

    cd /workdir/wikir
    pisa query <<< 'hello world'

We should get the results in the TREC format.

It is more useful to use the set of queries that come with the
collection. We can use the `ir_datasets` tool that comes pre-installed
in the Docker image. Notice that we use `awk` to convert the file format
to a format understood by PISA.

    ir_datasets export wikir/en1k/test queries \
        | awk '{ print $1":"$2 }' \
        > /data/wikir.queries

We can now run these queries against the index.

    pisa query < /data/wikir.queries

TODO(<https://github.com/pisa-engine/pisa/issues/608>): trec_eval

We can also run a benchmark, instead of returning results, by simply
passing `--benchmark` flag. It also may be more convenient to read the
queries from a file:

    pisa query --benchmark < /data/wikir.queries

This will print out some logs and a JSON result with some statistics.
Note that by default the algorithm used is `block_max_wand`, which is a
rather efficient disjunctive top-k retrieval algorithm. For now, let's
see how the results will compare when we use `ranked_or`, which is
exhaustive retrieval:

    pisa query --benchmark --algorithm ranked_or < /data/wikir.queries

This should be significantly slower. We can also try `ranked_and` for
_conjunctive_ retrieval, which requires _all_ query terms to be present
for a document to be returned:

    pisa query --benchmark --algorithm ranked_and < /data/wikir.queries

By default we will retrieve top 10 results, but we can choose a
different value:

    pisa query --benchmark -k 1000 < /data/wikir.queries

#### Multiple indexes

When building the index, we used default values for the encoding, skip
list block size, and the scorer. We can inspect those by running the
following command:

    pisa meta alias default

We can choose different parameters at indexing time, however, we can also
create an additional index using an alias.

    pisa add-index --alias interpolative --encoding block_interpolative

Notice that this time around it is much faster than before. This is
because much of work can be reused. The collection is already parsed and
an inverted index is built; we only need to compress its postings with a
different encoding.

We can now list aliases:

    pisa meta aliases

And inspect parameters of the new index:

    pisa meta alias interpolative

The `block_interpolative` encoding is known to be much more
space-efficient but slower. We can verify the size by listing the files:

    du -ah /workdir/wikir | grep inv

Finally, we can query the new index by passing the alias:

    pisa query --benchmark --alias interpolative < /data/queries.txt

Querying the new index should be slower than the initial one.

## Section 2: MS MARCO Experiments

TODO(<https://github.com/pisa-engine/pisa/issues/610>): collection & queries

    pisa index ciff \
        --input /workdir/msmarco.ciff \
        --output /workdir/msmarco \
        --scorer passthrough

Notice that we use `passthrough` scorer. This is because the payloads
that are available in the provided CIFF are precomputed scores as
opposed to frequencies, therefore we only want to add up the scores but
not use any particular formula such as BM25.

    cd /workdir/msmarco
    pisa query < /data/queries.txt # TODO(<https://github.com/pisa-engine/pisa/issues/610>)
    pisa query --benchmark < /data/queries.txt

TODO(<https://github.com/pisa-engine/pisa/issues/610>): These experiments
will also demonstrate the slowdowns caused by LSR in practice.
 
## Data: Finer Details

TODO(<https://github.com/pisa-engine/pisa/issues/611>)

You will see the following files:

* `metadata.yaml`: collection metadata
* `documents`: new-line separated document titles
* `terms`: new-line separated terms occurring in the collection
* `urls`: new-line separated URLs (empty lines for this example)
* `doclex`: document lexicon -- a mapping between document titles and
  numeric document IDs in a binary format
* `termlex`: term lexicon -- a mapping between terms and numeric term
  IDs in a binary format
* `fwd`: forward index
* `inv.docs`: inverted index (document IDs)
* `inv.freqs`: inverted index (frequencies)
* `inv.sizes`: document sizes (binary format)
* `inv:block_simdbp`: inverted index in a binary format
* `wdata:size=64:bm25:b=0.4:k1=0.9`: additional data, including max
  scores and skip lists
