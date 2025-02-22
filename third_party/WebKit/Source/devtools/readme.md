# Chrome DevTools frontend

The client-side of the Chrome DevTools, including all JS & CSS to run the DevTools webapp.

It is available on NPM as the [chrome-devtools-frontend](https://www.npmjs.com/package/chrome-devtools-frontend) package. It's not currently available via CJS or ES2015 modules, so consuming this package in other tools may require [some effort](https://github.com/paulirish/devtools-timeline-model/blob/master/index.js).

#### Package versioning
The version number of the npm package (e.g. `1.0.373466`) refers to the Chromium commit position of latest frontend git commit. It's incremented with every Chromium commit, however the package is updated roughly daily.

### Source code
The frontend is available through a git subtree mirror on [chromium.googlesource.com](https://chromium.googlesource.com/chromium/src/third_party/WebKit/Source/devtools/), with a regularly updating GitHub mirror at [github.com/ChromeDevTools/devtools-frontend](https://github.com/ChromeDevTools/devtools-frontend). The codebase's true location is in `third_party/WebKit/Source/devtools/` in [Chromium's git repo](https://chromium.googlesource.com/chromium/src/).

### Getting Started

1. Clone the repo
2. Go to repo root and run:  `npm start`
    - This launches Chrome Canary and starts the dev server with 1 command
3. Go to http://localhost:9222#custom=true&experiments=true

> **Power user tips:**
>
> You can customize the port for the dev server: e.g. `PORT=8888 npm start`.
>
> You can also launch chrome and start the server separately:
> - `npm run chrome`
> - `npm run server`
>
> When you start Chrome separately, you can pass extra args to Chrome:
> ```
> npm run chrome -- https://news.ycombinator.com
> ```
> (e.g. this launches Hacker News on startup)
>
> If you want to reset your development profile for Chrome, pass in "--reset-profile":
> ```
> npm start -- --reset-profile
> ```
> *OR*
> ```
> npm run chrome -- --reset-profile
> ```

### Hacking
* DevTools documentation: [devtools.chrome.com](https://devtools.chrome.com)
* [Debugging protocol docs](https://developer.chrome.com/devtools/docs/debugger-protocol) and [Chrome Debugging Protocol Viewer](http://chromedevtools.github.io/debugger-protocol-viewer/)
* [awesome-chrome-devtools](https://github.com/paulirish/awesome-chrome-devtools): recommended tools and resources
* Contributing to DevTools: [bit.ly/devtools-contribution-guide](http://bit.ly/devtools-contribution-guide)

### Useful Commands

Basic:
* `npm run format` - formats your code using clang-format
* `npm test` - builds devtools and runs all inspector layout tests

> Note: If you're using a full chromium checkout and compiled content shell in out/Release, then `npm test` uses that. Otherwise, with only a front-end checkout (i.e. cloning from GitHub), then `npm test` will fetch a previously compiled content shell from the cloud (and cache it for future test runs).

Advanced testing:
* `npm test -- --fetch-content-shell` - even if you're using a full chromium checkout and have a compiled content shell, this will fetch a pre-compiled content shell. This is useful if you haven't compiled your content shell recently.
* `npm test -- -f --child-processes=16` - pass in additional flags to the test harness
* `npm test -- inspector/sources inspector/console` - run specific tests
* `npm test -- inspector/cookie-resource-match.html --debug-devtools` OR `npm run debug-test inspector/cookie-resource-match.html` - debug a specific test (non-bundled & minified). You can use "-d" as a shorthand for "--debug-devtools".

#### Development
* All devtools commits: [View the log], [RSS feed] or [@DevToolsCommits] on Twitter
* [All open DevTools tickets] on crbug.com
* File a new DevTools ticket: [new.crbug.com](https://bugs.chromium.org/p/chromium/issues/entry?labels=OS-All,Type-Bug,Pri-2&components=Platform%3EDevTools)
* Code reviews mailing list: [devtools-reviews@chromium.org]

### Getting in touch
* [@ChromeDevTools] on Twitter
* Chrome DevTools mailing list: [groups.google.com/forum/google-chrome-developer-tools](https://groups.google.com/forum/#!forum/google-chrome-developer-tools)

  [devtools-reviews@chromium.org]: https://groups.google.com/a/chromium.org/forum/#!forum/devtools-reviews
  [RSS feed]: https://feeds.peter.sh/chrome-devtools/
  [View the log]: https://chromium.googlesource.com/chromium/src/third_party/WebKit/Source/devtools/+log/master
  [@ChromeDevTools]: http://twitter.com/ChromeDevTools
  [@DevToolsCommits]: http://twitter.com/DevToolsCommits
  [all open DevTools tickets]: https://bugs.chromium.org/p/chromium/issues/list?can=2&q=component%3APlatform%3EDevTools&sort=&groupby=&colspec=ID+Stars+Owner+Summary+Modified+Opened
