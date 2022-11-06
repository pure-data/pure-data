### pdmanual-hugo

The Pd Manual exported to `.md` files and then built to an `HTML` site via [HUGO](https://gohugo.io/)

To build the manual:

1. Install HUGO --> https://gohugo.io/getting-started/installing/
1. on a terminal `cd` to this folder.
1. run the `hugo` command
1. you should get something like

```
Start building sites …
hugo v0.89.4-AB01BA6E windows/amd64 BuildDate=2021-11-17T08:24:09Z VendorInfo=gohugoio

                   | EN
-------------------+-----
  Pages            | 10
  Paginator pages  |  0
  Non-page files   |  0
  Static files     | 43
  Processed images |  0
  Aliases          |  0
  Sitemaps         |  0
  Cleaned          |  0

Total in 111 ms
```
now a `public` folder has been created in the local repo. You can browse `index.html`

You can also run command `hugo server` and browse files via `http://localhost:1313/` in your web-browser. If you edit the `.md` files on the `doc/1.manual/markdown` folder changes show-up live.

