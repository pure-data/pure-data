Notes for translators
=====================

The Pd GUI interface is available in multiple languages.

If support for your language is only partial or missing completely,
you are invited to join the Pd translation effort.

# Project mailinglist

We don't have a specific mailinglist for translating Pd,
so we use the ordinary Pd mailinglist:

  https://lists.puredata.info/listinfo/pd-list

Feel free to subscribe to the mailinglist to discuss translation issues.

# Different translation workflows

There are two ways to manage the translations:

- using the Weblate web application at

  https://hosted.weblate.org/projects/pure-data/

- using the Git repository to grab and commit PO files

## Translation with Weblate

If you decide to use Weblate, you don't have to learn Git usage.
Get in touch with us on the above mailing list so that we can setup
the initial translation files for you, and once they appear
on Weblate, you can start working!
You should communicate us the ISO code of your translation
(see "How to start a new translation" for more information on possible ISO codes).

If you want to retrieve the content of Weblate via git you can use
this git repository as remote:

```
$ git remote add weblate https://hosted.weblate.org/git/pure-data/pure-data
```

## Translation without Weblate

---------------------------

If you're not using Weblate, you'll have to interact with the Git
repository. So read README.git first. Really. And then please respect the
guidelines below.

Write meaningful commit messages. Always start with the language
code of the affected translations. Some examples of good commit
messages:

* it: Translated menus
* pt_br: Complete translation of deken


# How to start a new translation

See the `po/README.txt` for adding a new translation manually.


In any case, you must find out the language code for your translation
(it looks like `fr` or `de_at`).

See https://www.gnu.org/software/gettext/manual/html_node/Usual-Language-Codes.html
for a list of such codes.
