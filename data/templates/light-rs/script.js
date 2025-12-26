function updatePage(data) {
    const nowSingingElem = document.getElementById("now-singing");
    if (nowSingingElem) {
        if (data.singing.length > 1) {
            nowSingingElem.innerHTML = `<span class="song-name" style="white-space: nowrap;">${data.singing[0]}</span> <span class="timestamp" style="font-size: 0.8em; vertical-align: middle;">${data.singing[1]}</span>`;
        } else {
            nowSingingElem.textContent = data.singing[0];
        }
    }
}

async function fetchInitialData() {
    try {
        const response = await fetch("http://127.0.0.1:8080/data");
        if (!response.ok) {
            return;
        }
        const data = await response.json();
        updatePage(data);
    } catch (error) {
        console.error("Failed to fetch data", error);
    }
}

window.addEventListener("load", () => {
    fetchInitialData();
    try {
        const evtSource = new EventSource("http://127.0.0.1:8080/events");
        evtSource.onmessage = function (event) {
            const data = JSON.parse(event.data);
            updatePage(data);
        };
        evtSource.onerror = function (_) {
            evtSource.close();
        };
    } catch (e) {
        console.log("EventSource skipped.");
    }

    const nowSingingElem = document.getElementById("now-singing");

    let nowSingingInterval;

    if (nowSingingElem) {
        startScrollHorizontal(nowSingingElem, nowSingingInterval);
    }
});

function autoScrollHorizontal(element, interval) {
    element.scrollLeft += 1;

    if (element.scrollLeft >= element.scrollWidth - element.clientWidth) {
        clearInterval(interval);

        setTimeout(() => {
            element.scrollLeft = 0;
            setTimeout(() => {
                startScrollHorizontal(element, interval);
            }, 1500);
        }, 3000);
    }
}

function startScrollHorizontal(element, interval) {
    if (interval) clearInterval(interval);
    interval = setInterval(() => autoScrollHorizontal(element, interval), 20);
}
