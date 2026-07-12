function updatePage(data) {
    const nowSingingElem = document.getElementById("now-singing");
    if (nowSingingElem) {
        nowSingingElem.textContent = data.singing[0];
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
    function connectWebSocket() {
        try {
            const ws = new WebSocket("ws://127.0.0.1:8080/ws");
            ws.onmessage = function (event) {
                const data = JSON.parse(event.data);
                updatePage(data);
            };
            ws.onclose = function () {
                setTimeout(connectWebSocket, 3000);
            };
            ws.onerror = function (_) {
                ws.close();
            };
        } catch (e) {
            console.log("WebSocket skipped.");
        }
    }
    connectWebSocket();

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
