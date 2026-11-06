/* Scene geometry designer + console cell viewer (custom Canvas, no three.js). */
(function (global) {
    "use strict";

    function defaultFloorRect(sceneSize) {
        var size = sceneSize > 0 ? sceneSize : 1;
        var half = size * 0.35;
        return {
            y: 0,
            height: 0.3,
            walls: [
                { x1: -half, z1: -half, x2: half, z2: -half },
                { x1: half, z1: -half, x2: half, z2: half },
                { x1: half, z1: half, x2: -half, z2: half },
                { x1: -half, z1: half, x2: -half, z2: -half }
            ],
            windows: [],
            doors: []
        };
    }

    function defaultModel() {
        return {
            sceneSize: 1.0,
            wallHeight: 0.15,
            floors: [defaultFloorRect(1.0)]
        };
    }

    function cloneJson(value) {
        return JSON.parse(JSON.stringify(value));
    }

    function clamp01(value) {
        if (value < 0) {
            return 0;
        }
        if (value > 1) {
            return 1;
        }
        return value;
    }

    function distPointToSeg(px, pz, seg) {
        var ax = seg.x1;
        var az = seg.z1;
        var bx = seg.x2;
        var bz = seg.z2;
        var abx = bx - ax;
        var abz = bz - az;
        var len2 = abx * abx + abz * abz;
        if (len2 < 1e-12) {
            var dx0 = px - ax;
            var dz0 = pz - az;
            return Math.sqrt(dx0 * dx0 + dz0 * dz0);
        }
        var t = ((px - ax) * abx + (pz - az) * abz) / len2;
        if (t < 0) {
            t = 0;
        } else if (t > 1) {
            t = 1;
        }
        var qx = ax + abx * t;
        var qz = az + abz * t;
        var dx = px - qx;
        var dz = pz - qz;
        return Math.sqrt(dx * dx + dz * dz);
    }

    function projectOnSeg(px, pz, seg) {
        var ax = seg.x1;
        var az = seg.z1;
        var bx = seg.x2;
        var bz = seg.z2;
        var abx = bx - ax;
        var abz = bz - az;
        var len2 = abx * abx + abz * abz;
        if (len2 < 1e-12) {
            return { x: ax, z: az, t: 0 };
        }
        var t = ((px - ax) * abx + (pz - az) * abz) / len2;
        if (t < 0) {
            t = 0;
        } else if (t > 1) {
            t = 1;
        }
        return { x: ax + abx * t, z: az + abz * t, t: t };
    }

    function projectOrbit(point, yaw, pitch, distance, width, height) {
        var cy = Math.cos(yaw);
        var sy = Math.sin(yaw);
        var cp = Math.cos(pitch);
        var sp = Math.sin(pitch);
        var x = point.x;
        var y = point.y;
        var z = point.z;
        var rx = x * cy + z * sy;
        var rz = -x * sy + z * cy;
        var ry = y * cp - rz * sp;
        var rz2 = y * sp + rz * cp;
        var depth = distance + rz2;
        if (depth < 0.05) {
            depth = 0.05;
        }
        var scale = Math.min(width, height) * 0.55 / depth;
        return {
            x: width * 0.5 + rx * scale * (width / Math.min(width, height)),
            y: height * 0.55 - ry * scale * (height / Math.min(width, height))
        };
    }

    function SceneDesigner(canvas) {
        this.canvas = canvas;
        this.ctx = canvas.getContext("2d");
        this.model = defaultModel();
        this.floorIndex = 0;
        this.tool = "wall";
        this.viewMode = false;
        this.pendingPoint = null;
        this.yaw = 0.7;
        this.pitch = 0.45;
        this.dragOrbit = false;
        this.dragHandle = null;
        this.lastX = 0;
        this.lastY = 0;
        this.onChange = null;
        this.selection = null;
        this.bindEvents();
        this.resize();
    }

    SceneDesigner.prototype.bindEvents = function () {
        var self = this;
        this.canvas.addEventListener("mousedown", function (event) {
            self.onPointerDown(event);
        });
        this.canvas.addEventListener("mousemove", function (event) {
            self.onPointerMove(event);
        });
        window.addEventListener("mouseup", function () {
            self.dragOrbit = false;
            if (self.dragHandle) {
                self.dragHandle = null;
                self.notify();
            }
        });
        this.canvas.addEventListener("dblclick", function (event) {
            self.onDoubleClick(event);
        });
        this.canvas.addEventListener("wheel", function (event) {
            if (!self.viewMode) {
                return;
            }
            event.preventDefault();
            self.pitch = Math.max(0.15, Math.min(1.2, self.pitch + event.deltaY * 0.0015));
            self.draw();
        }, { passive: false });
    };

    SceneDesigner.prototype.resize = function () {
        var parent = this.canvas.parentElement;
        var width = parent ? parent.clientWidth : 640;
        var height = parent ? Math.max(320, parent.clientHeight || 420) : 420;
        this.canvas.width = Math.max(320, width);
        this.canvas.height = height;
        this.draw();
    };

    SceneDesigner.prototype.setModel = function (model) {
        this.model = model && model.floors ? cloneJson(model) : defaultModel();
        if (!this.model.floors.length) {
            this.model.floors = defaultModel().floors;
        }
        this.floorIndex = 0;
        this.pendingPoint = null;
        this.selection = null;
        this.dragHandle = null;
        this.draw();
    };

    SceneDesigner.prototype.getModel = function () {
        return cloneJson(this.model);
    };

    SceneDesigner.prototype.setTool = function (tool) {
        this.tool = tool || "wall";
        this.pendingPoint = null;
        if ((tool === "window" || tool === "door") && this.placeOpeningOnSelectedWall(tool)) {
            this.draw();
            return true;
        }
        this.draw();
        return false;
    };

    SceneDesigner.prototype.setViewMode = function (enabled) {
        this.viewMode = !!enabled;
        this.pendingPoint = null;
        this.dragHandle = null;
        this.draw();
    };

    SceneDesigner.prototype.toggleViewMode = function () {
        this.setViewMode(!this.viewMode);
        return this.viewMode;
    };

    SceneDesigner.prototype.addFloor = function () {
        var last = this.model.floors[this.model.floors.length - 1];
        var floor = defaultFloorRect(this.model.sceneSize || 1);
        floor.y = last ? (last.y + (last.height || 0.3)) : 0;
        this.model.floors.push(floor);
        this.floorIndex = this.model.floors.length - 1;
        this.selection = { list: "walls", index: 0 };
        this.pendingPoint = null;
        this.notify();
        this.draw();
    };

    SceneDesigner.prototype.placeOpeningOnSelectedWall = function (kind) {
        if (!this.selection || this.selection.list !== "walls") {
            return false;
        }
        var floor = this.currentFloor();
        var wall = floor.walls[this.selection.index];
        if (!wall) {
            return false;
        }
        var dx = wall.x2 - wall.x1;
        var dz = wall.z2 - wall.z1;
        var len = Math.sqrt(dx * dx + dz * dz);
        if (len < 1e-6) {
            return false;
        }
        var openingLen = Math.min(len * 0.35, (this.model.sceneSize || 1) / 7);
        if (openingLen < len * 0.08) {
            openingLen = len * 0.08;
        }
        var ux = dx / len;
        var uz = dz / len;
        var midX = (wall.x1 + wall.x2) * 0.5;
        var midZ = (wall.z1 + wall.z2) * 0.5;
        var half = openingLen * 0.5;
        var seg = {
            x1: midX - ux * half,
            z1: midZ - uz * half,
            x2: midX + ux * half,
            z2: midZ + uz * half
        };
        var listName = kind === "door" ? "doors" : "windows";
        floor[listName].push(seg);
        this.selection = { list: listName, index: floor[listName].length - 1 };
        this.notify();
        return true;
    };

    SceneDesigner.prototype.removeSelected = function () {
        if (this.selection) {
            var floor = this.currentFloor();
            var list = floor[this.selection.list];
            if (list && this.selection.index >= 0 && this.selection.index < list.length) {
                list.splice(this.selection.index, 1);
                this.selection = null;
                this.notify();
                this.draw();
                return true;
            }
        }
        if (this.model.floors.length > 1) {
            this.model.floors.splice(this.floorIndex, 1);
            if (this.floorIndex >= this.model.floors.length) {
                this.floorIndex = this.model.floors.length - 1;
            }
            this.selection = null;
            this.notify();
            this.draw();
            return true;
        }
        return false;
    };

    SceneDesigner.prototype.currentFloor = function () {
        return this.model.floors[this.floorIndex] || this.model.floors[0];
    };

    SceneDesigner.prototype.worldFromEvent = function (event) {
        var rect = this.canvas.getBoundingClientRect();
        var px = (event.clientX - rect.left) / rect.width;
        var py = (event.clientY - rect.top) / rect.height;
        var size = this.model.sceneSize || 1;
        return {
            x: (px - 0.5) * size,
            z: (py - 0.5) * size
        };
    };

    SceneDesigner.prototype.handleHitSize = function () {
        return (this.model.sceneSize || 1) * 0.028;
    };

    SceneDesigner.prototype.findHandleAt = function (point) {
        var floor = this.currentFloor();
        var threshold = this.handleHitSize();
        var best = null;
        var bestDist = threshold;
        ["walls", "windows", "doors"].forEach(function (listName) {
            var list = floor[listName] || [];
            list.forEach(function (seg, index) {
                [
                    { end: 1, x: seg.x1, z: seg.z1 },
                    { end: 2, x: seg.x2, z: seg.z2 }
                ].forEach(function (handle) {
                    var dx = handle.x - point.x;
                    var dz = handle.z - point.z;
                    var dist = Math.sqrt(dx * dx + dz * dz);
                    if (dist < bestDist) {
                        bestDist = dist;
                        best = { list: listName, index: index, end: handle.end };
                    }
                });
            });
        });
        return best;
    };

    SceneDesigner.prototype.snapJoin = function (point, ignoreHandle) {
        var floor = this.currentFloor();
        var threshold = (this.model.sceneSize || 1) * 0.03;
        var best = null;
        var bestDist = threshold;
        ["walls", "windows", "doors"].forEach(function (listName) {
            var list = floor[listName] || [];
            list.forEach(function (seg, index) {
                [
                    { end: 1, x: seg.x1, z: seg.z1 },
                    { end: 2, x: seg.x2, z: seg.z2 }
                ].forEach(function (endpoint) {
                    if (ignoreHandle
                        && ignoreHandle.list === listName
                        && ignoreHandle.index === index
                        && ignoreHandle.end === endpoint.end) {
                        return;
                    }
                    var dx = endpoint.x - point.x;
                    var dz = endpoint.z - point.z;
                    var dist = Math.sqrt(dx * dx + dz * dz);
                    if (dist < bestDist) {
                        bestDist = dist;
                        best = { x: endpoint.x, z: endpoint.z };
                    }
                });
            });
        });
        return best || point;
    };

    SceneDesigner.prototype.selectNear = function (point) {
        var floor = this.currentFloor();
        var threshold = (this.model.sceneSize || 1) * 0.025;
        var best = null;
        var bestDist = threshold;
        ["walls", "windows", "doors"].forEach(function (listName) {
            var list = floor[listName] || [];
            list.forEach(function (seg, index) {
                var dist = distPointToSeg(point.x, point.z, seg);
                if (dist < bestDist) {
                    bestDist = dist;
                    best = { list: listName, index: index };
                }
            });
        });
        this.selection = best;
        this.draw();
        return best;
    };

    SceneDesigner.prototype.insertVertexOnWall = function (point) {
        var floor = this.currentFloor();
        var threshold = (this.model.sceneSize || 1) * 0.03;
        var bestIndex = -1;
        var bestDist = threshold;
        var bestProj = null;
        (floor.walls || []).forEach(function (seg, index) {
            var dist = distPointToSeg(point.x, point.z, seg);
            if (dist < bestDist) {
                bestDist = dist;
                bestIndex = index;
                bestProj = projectOnSeg(point.x, point.z, seg);
            }
        });
        if (bestIndex < 0 || !bestProj || bestProj.t <= 0.05 || bestProj.t >= 0.95) {
            return false;
        }
        var wall = floor.walls[bestIndex];
        var second = { x1: bestProj.x, z1: bestProj.z, x2: wall.x2, z2: wall.z2 };
        wall.x2 = bestProj.x;
        wall.z2 = bestProj.z;
        floor.walls.splice(bestIndex + 1, 0, second);
        this.selection = { list: "walls", index: bestIndex + 1 };
        this.notify();
        this.draw();
        return true;
    };

    SceneDesigner.prototype.onDoubleClick = function (event) {
        if (this.viewMode) {
            return;
        }
        event.preventDefault();
        var point = this.worldFromEvent(event);
        this.insertVertexOnWall(point);
    };

    SceneDesigner.prototype.onPointerDown = function (event) {
        if (this.viewMode) {
            this.dragOrbit = true;
            this.lastX = event.clientX;
            this.lastY = event.clientY;
            return;
        }
        var point = this.worldFromEvent(event);
        var handle = this.findHandleAt(point);
        if (handle) {
            this.dragHandle = handle;
            this.selection = { list: handle.list, index: handle.index };
            this.pendingPoint = null;
            this.draw();
            return;
        }
        if (this.tool === "window" || this.tool === "door") {
            var wallHit = this.selectNear(point);
            if (wallHit && wallHit.list === "walls") {
                this.placeOpeningOnSelectedWall(this.tool);
                this.draw();
                return;
            }
            this.selection = wallHit;
            this.draw();
            return;
        }
        if (this.tool !== "wall") {
            this.selectNear(point);
            return;
        }
        if (!this.pendingPoint) {
            var hit = this.selectNear(point);
            if (hit) {
                return;
            }
            this.pendingPoint = this.snapJoin(point);
            this.selection = null;
            this.draw();
            return;
        }
        var start = this.pendingPoint;
        this.pendingPoint = null;
        point = this.snapJoin(point);
        var defaultLen = (this.model.sceneSize || 1) / 7;
        var dx = point.x - start.x;
        var dz = point.z - start.z;
        var len = Math.sqrt(dx * dx + dz * dz);
        if (len < defaultLen * 0.15) {
            point = {
                x: start.x + defaultLen,
                z: start.z
            };
        }
        var floor = this.currentFloor();
        floor.walls.push({ x1: start.x, z1: start.z, x2: point.x, z2: point.z });
        this.selection = { list: "walls", index: floor.walls.length - 1 };
        this.notify();
        this.draw();
    };

    SceneDesigner.prototype.onPointerMove = function (event) {
        if (this.viewMode && this.dragOrbit) {
            var dx = event.clientX - this.lastX;
            var dy = event.clientY - this.lastY;
            this.lastX = event.clientX;
            this.lastY = event.clientY;
            this.yaw += dx * 0.01;
            this.pitch = Math.max(0.15, Math.min(1.2, this.pitch + dy * 0.01));
            this.draw();
            return;
        }
        if (!this.dragHandle || this.viewMode) {
            return;
        }
        var floor = this.currentFloor();
        var list = floor[this.dragHandle.list];
        if (!list || !list[this.dragHandle.index]) {
            return;
        }
        var point = this.snapJoin(this.worldFromEvent(event), this.dragHandle);
        var seg = list[this.dragHandle.index];
        if (this.dragHandle.end === 1) {
            seg.x1 = point.x;
            seg.z1 = point.z;
        } else {
            seg.x2 = point.x;
            seg.z2 = point.z;
        }
        this.draw();
    };

    SceneDesigner.prototype.notify = function () {
        if (typeof this.onChange === "function") {
            this.onChange();
        }
    };

    SceneDesigner.prototype.drawTop = function () {
        var ctx = this.ctx;
        var width = this.canvas.width;
        var height = this.canvas.height;
        var size = this.model.sceneSize || 1;
        ctx.fillStyle = "#1b1f2a";
        ctx.fillRect(0, 0, width, height);
        ctx.strokeStyle = "#2e3648";
        ctx.lineWidth = 1;
        for (var g = 0; g <= 10; g++) {
            var gx = (g / 10) * width;
            var gy = (g / 10) * height;
            ctx.beginPath();
            ctx.moveTo(gx, 0);
            ctx.lineTo(gx, height);
            ctx.stroke();
            ctx.beginPath();
            ctx.moveTo(0, gy);
            ctx.lineTo(width, gy);
            ctx.stroke();
        }
        var self = this;
        function toPx(point) {
            return {
                x: ((point.x / size) + 0.5) * width,
                y: ((point.z / size) + 0.5) * height
            };
        }
        function drawFloorPlate(floor, active) {
            var walls = floor.walls || [];
            if (!walls.length) {
                return;
            }
            ctx.beginPath();
            walls.forEach(function (seg, index) {
                var a = toPx({ x: seg.x1, z: seg.z1 });
                if (index === 0) {
                    ctx.moveTo(a.x, a.y);
                }
                var b = toPx({ x: seg.x2, z: seg.z2 });
                ctx.lineTo(b.x, b.y);
            });
            ctx.closePath();
            ctx.fillStyle = active ? "rgba(100, 140, 180, 0.22)" : "rgba(70, 90, 110, 0.12)";
            ctx.fill();
        }
        this.model.floors.forEach(function (floor, index) {
            drawFloorPlate(floor, index === self.floorIndex);
        });
        function drawSegs(list, listName, color, widthPx) {
            (list || []).forEach(function (seg, index) {
                var selected = self.selection
                    && self.selection.list === listName
                    && self.selection.index === index;
                var a = toPx({ x: seg.x1, z: seg.z1 });
                var b = toPx({ x: seg.x2, z: seg.z2 });
                ctx.strokeStyle = selected ? "#ce93d8" : color;
                ctx.lineWidth = selected ? widthPx + 2 : widthPx;
                ctx.beginPath();
                ctx.moveTo(a.x, a.y);
                ctx.lineTo(b.x, b.y);
                ctx.stroke();
            });
        }
        function drawHandles(list, listName, color) {
            (list || []).forEach(function (seg, index) {
                var selected = self.selection
                    && self.selection.list === listName
                    && self.selection.index === index;
                var show = listName === "walls" || selected;
                if (!show) {
                    return;
                }
                [
                    { x: seg.x1, z: seg.z1 },
                    { x: seg.x2, z: seg.z2 }
                ].forEach(function (endpoint) {
                    var p = toPx(endpoint);
                    var half = 5;
                    ctx.fillStyle = selected ? "#ce93d8" : color;
                    ctx.strokeStyle = "#111";
                    ctx.lineWidth = 1;
                    ctx.fillRect(p.x - half, p.y - half, half * 2, half * 2);
                    ctx.strokeRect(p.x - half, p.y - half, half * 2, half * 2);
                });
            });
        }
        var floor = this.currentFloor();
        drawSegs(floor.walls, "walls", "#d7dee8", 3);
        drawSegs(floor.windows, "windows", "#4fc3f7", 2);
        drawSegs(floor.doors, "doors", "#ffb74d", 2);
        drawHandles(floor.walls, "walls", "#eceff1");
        drawHandles(floor.windows, "windows", "#4fc3f7");
        drawHandles(floor.doors, "doors", "#ffb74d");
        if (this.pendingPoint) {
            var p = toPx(this.pendingPoint);
            ctx.fillStyle = "#81c784";
            ctx.beginPath();
            ctx.arc(p.x, p.y, 5, 0, Math.PI * 2);
            ctx.fill();
        }
        ctx.fillStyle = "#9aa4b5";
        ctx.font = "12px sans-serif";
        ctx.fillText(
            "Floor " + (this.floorIndex + 1) + "/" + this.model.floors.length + " · tool: " + this.tool,
            10,
            18
        );
    };

    SceneDesigner.prototype.drawView = function () {
        var ctx = this.ctx;
        var width = this.canvas.width;
        var height = this.canvas.height;
        ctx.fillStyle = "#12151c";
        ctx.fillRect(0, 0, width, height);
        var wallHeight = this.model.wallHeight || 0.15;
        var distance = 2.2;
        var yaw = this.yaw;
        var pitch = this.pitch;
        this.model.floors.forEach(function (floor) {
            var baseY = floor.y || 0;
            function edge(seg, color) {
                var a1 = projectOrbit({ x: seg.x1, y: baseY, z: seg.z1 }, yaw, pitch, distance, width, height);
                var a2 = projectOrbit({ x: seg.x2, y: baseY, z: seg.z2 }, yaw, pitch, distance, width, height);
                var b1 = projectOrbit({ x: seg.x1, y: baseY + wallHeight, z: seg.z1 }, yaw, pitch, distance, width, height);
                var b2 = projectOrbit({ x: seg.x2, y: baseY + wallHeight, z: seg.z2 }, yaw, pitch, distance, width, height);
                ctx.strokeStyle = color;
                ctx.lineWidth = 1.5;
                ctx.beginPath();
                ctx.moveTo(a1.x, a1.y);
                ctx.lineTo(a2.x, a2.y);
                ctx.lineTo(b2.x, b2.y);
                ctx.lineTo(b1.x, b1.y);
                ctx.closePath();
                ctx.stroke();
            }
            function plate(walls, fill) {
                if (!walls || !walls.length) {
                    return;
                }
                ctx.beginPath();
                walls.forEach(function (seg, index) {
                    var p = projectOrbit({ x: seg.x1, y: baseY, z: seg.z1 }, yaw, pitch, distance, width, height);
                    if (index === 0) {
                        ctx.moveTo(p.x, p.y);
                    } else {
                        ctx.lineTo(p.x, p.y);
                    }
                });
                ctx.closePath();
                ctx.fillStyle = fill;
                ctx.fill();
            }
            plate(floor.walls, "rgba(96,125,139,0.25)");
            (floor.walls || []).forEach(function (seg) { edge(seg, "#cfd8dc"); });
            (floor.windows || []).forEach(function (seg) { edge(seg, "#4fc3f7"); });
            (floor.doors || []).forEach(function (seg) { edge(seg, "#ffb74d"); });
        });
        ctx.fillStyle = "#9aa4b5";
        ctx.font = "12px sans-serif";
        ctx.fillText("View scene · drag to orbit", 10, 18);
    };

    SceneDesigner.prototype.draw = function () {
        if (this.viewMode) {
            this.drawView();
        } else {
            this.drawTop();
        }
    };

    SceneDesigner.prototype.bakeJpegBlob = function (quality) {
        var width = 1920;
        var height = 1080;
        var off = document.createElement("canvas");
        off.width = width;
        off.height = height;
        var ctx = off.getContext("2d");
        ctx.fillStyle = "#12151c";
        ctx.fillRect(0, 0, width, height);
        var wallHeight = this.model.wallHeight || 0.15;
        var yaw = 0.75;
        var pitch = 0.42;
        var distance = 2.1;
        this.model.floors.forEach(function (floor) {
            var baseY = floor.y || 0;
            function edge(seg, color, fill) {
                var a1 = projectOrbit({ x: seg.x1, y: baseY, z: seg.z1 }, yaw, pitch, distance, width, height);
                var a2 = projectOrbit({ x: seg.x2, y: baseY, z: seg.z2 }, yaw, pitch, distance, width, height);
                var b1 = projectOrbit({ x: seg.x1, y: baseY + wallHeight, z: seg.z1 }, yaw, pitch, distance, width, height);
                var b2 = projectOrbit({ x: seg.x2, y: baseY + wallHeight, z: seg.z2 }, yaw, pitch, distance, width, height);
                ctx.beginPath();
                ctx.moveTo(a1.x, a1.y);
                ctx.lineTo(a2.x, a2.y);
                ctx.lineTo(b2.x, b2.y);
                ctx.lineTo(b1.x, b1.y);
                ctx.closePath();
                if (fill) {
                    ctx.fillStyle = fill;
                    ctx.fill();
                }
                ctx.strokeStyle = color;
                ctx.lineWidth = 2;
                ctx.stroke();
            }
            function plate(walls) {
                if (!walls || !walls.length) {
                    return;
                }
                ctx.beginPath();
                walls.forEach(function (seg, index) {
                    var p = projectOrbit({ x: seg.x1, y: baseY, z: seg.z1 }, yaw, pitch, distance, width, height);
                    if (index === 0) {
                        ctx.moveTo(p.x, p.y);
                    } else {
                        ctx.lineTo(p.x, p.y);
                    }
                });
                ctx.closePath();
                ctx.fillStyle = "rgba(96,125,139,0.3)";
                ctx.fill();
            }
            plate(floor.walls);
            (floor.walls || []).forEach(function (seg) { edge(seg, "#eceff1", "rgba(96,125,139,0.35)"); });
            (floor.windows || []).forEach(function (seg) { edge(seg, "#4fc3f7", "rgba(79,195,247,0.2)"); });
            (floor.doors || []).forEach(function (seg) { edge(seg, "#ffb74d", "rgba(255,183,77,0.2)"); });
        });
        return new Promise(function (resolve) {
            off.toBlob(function (blob) {
                resolve(blob);
            }, "image/jpeg", quality == null ? 0.82 : quality);
        });
    };

    function mountSceneCell(host, sceneId, options) {
        options = options || {};
        host.classList.add("scene-cell");
        host.innerHTML = "";
        var frame = document.createElement("div");
        frame.className = "scene-cell-frame";
        var img = document.createElement("img");
        img.className = "scene-cell-bake";
        img.alt = "Scene";
        var overlays = document.createElement("div");
        overlays.className = "scene-cell-overlays";
        frame.appendChild(img);
        frame.appendChild(overlays);
        host.appendChild(frame);
        if (!sceneId) {
            host.classList.add("scene-cell--empty");
            host.textContent = "No scene";
            return;
        }
        fetch("/api/scenes/view?id=" + encodeURIComponent(sceneId), { cache: "no-store" })
            .then(function (response) {
                if (!response.ok) {
                    throw new Error("scene view failed");
                }
                return response.json();
            })
            .then(function (view) {
                img.src = "/api/scenes/bake?id=" + encodeURIComponent(sceneId) + "&t=" + Date.now();
                overlays.innerHTML = "";
                (view.pins || []).forEach(function (pin) {
                    var pinEl = document.createElement("div");
                    pinEl.className = "scene-cell-pin scene-cell-pin--" + (pin.viewStyle === "gauge" ? "gauge" : "icon");
                    pinEl.style.left = (clamp01(pin.x) * 100) + "%";
                    pinEl.style.top = (clamp01(pin.y) * 100) + "%";
                    if (typeof options.renderPin === "function") {
                        options.renderPin(pinEl, pin);
                    } else {
                        pinEl.title = pin.kind || "pin";
                    }
                    overlays.appendChild(pinEl);
                });
            })
            .catch(function () {
                host.classList.add("scene-cell--empty");
                host.textContent = "Scene unavailable";
            });
    }

    global.SceneKit = {
        defaultModel: defaultModel,
        SceneDesigner: SceneDesigner,
        mountSceneCell: mountSceneCell,
        clamp01: clamp01
    };
})(window);
