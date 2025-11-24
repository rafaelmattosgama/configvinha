self.addEventListener("install", (event) => {
    event.waitUntil(
        caches.open("vinha-cache-v1").then((cache) => {
            return cache.addAll([
                "/",
                "index.html",
                "style.css",
                "app.js",
                "manifest.json",
                "icon192.png",
                "icon512.png"
            ]);
        })
    );
});
self.addEventListener("fetch", (event) => {
    event.respondWith(
        caches.match(event.request).then((resp) => resp || fetch(event.request))
    );
});
