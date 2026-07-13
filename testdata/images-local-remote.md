# Local + remote images

Mix of local relative-path images and remote http(s) URLs. MiniMD's image loader is local-only - no network code - so the local ones below should display and every `http(s)://` reference is expected to be silently skipped rather than shown (same as a broken path - see `images-local.md`).

## Local image

![local red square](images/local-red.png "from testdata/images")

## Remote image (Wikimedia Commons - stable, long-lived URL)

![Wikipedia logo](https://upload.wikimedia.org/wikipedia/commons/6/63/Wikipedia-logo.png "fetched over the network")

## Another local image, right after the remote one

![local gradient](images/local-gradient.jpg)

## Remote image, pinned to a specific commit (won't change out from under this file)

![GitHub's markdown topic icon](https://raw.githubusercontent.com/github/explore/80688e429a7d4ef2fca1e82350fe8e3517d3494/topics/markdown/markdown.png)

## Local and remote images in the same paragraph

Two side by side: ![local](images/local-red.png) and ![remote](https://upload.wikimedia.org/wikipedia/commons/6/63/Wikipedia-logo.png) referenced right next to each other.

## Remote image inside a link

[![remote image wrapped in a link](https://upload.wikimedia.org/wikipedia/commons/6/63/Wikipedia-logo.png)](https://www.wikipedia.org)
