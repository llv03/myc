(function () {
  var pathname = location.pathname || "";
  var path = (pathname.split("/").pop() || "index.html").toLowerCase();
  if (!path || path === "") path = "index.html";
  var inDocs = /\/docs(\/|$)/i.test(pathname) || pathname.indexOf("/docs/") !== -1;

  // Fallback for file:// URLs: check if path is under a docs folder via href context
  if (!inDocs) {
    try {
      var here = location.href || "";
      inDocs = /\/docs\//i.test(here) || /\/docs$/i.test(here.replace(/\/[^/]*$/, ""));
    } catch (e) {}
  }

  var links = document.querySelectorAll("nav a[href]");
  for (var i = 0; i < links.length; i++) {
    var a = links[i];
    var href = (a.getAttribute("href") || "").toLowerCase();
    var label = (a.textContent || "").trim().toLowerCase();
    var isCurrent = false;

    if (label === "docs" && inDocs) {
      isCurrent = true;
    } else if (!inDocs && href === path) {
      isCurrent = true;
    } else if (!inDocs && href.indexOf(path) !== -1 && path !== "index.html") {
      // exact filename match already handled; skip fuzzy
    }

    if (isCurrent) {
      a.className = (a.className + " current").trim();
    }
  }

  // Mark doc-nav current link by filename
  var docLinks = document.querySelectorAll(".doc-nav a[href]");
  for (var j = 0; j < docLinks.length; j++) {
    var dh = (docLinks[j].getAttribute("href") || "").toLowerCase();
    if (dh === path) {
      docLinks[j].className = (docLinks[j].className + " current").trim();
    }
  }

  var toggle = document.querySelector(".nav-toggle");
  var nav = document.querySelector("nav");
  if (toggle && nav) {
    toggle.addEventListener("click", function () {
      nav.classList.toggle("open");
    });
  }
})();
