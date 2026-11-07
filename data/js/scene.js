/* Scene geometry designer + console cell viewer (2D plan; three.js loaded on demand for View/bake). */
(function (global) {
    "use strict";

    var DEFAULT_FLOOR_COLOR = "#6a8fad";
    var DEFAULT_WALL_THICKNESS = 0.01;
    var DEFAULT_WALL_HEIGHT = 0.15;
    // Pixels spanning sceneSize at editZoom=1 — independent of canvas size (resize = more workspace).
    var PLAN_BASE_PX = 520;
    var THREE_SCRIPT_SRC = "/js/three.min.js";
    var threeJsLoadPromise = null;

    function loadThreeJs() {
        if (global.THREE) {
            return Promise.resolve(global.THREE);
        }
        if (threeJsLoadPromise) {
            return threeJsLoadPromise;
        }
        threeJsLoadPromise = new Promise(function (resolve, reject) {
            var existing = document.querySelector('script[data-scene-three="1"]');
            if (existing && global.THREE) {
                resolve(global.THREE);
                return;
            }
            var script = document.createElement("script");
            script.src = THREE_SCRIPT_SRC;
            script.async = true;
            script.setAttribute("data-scene-three", "1");
            script.onload = function () {
                if (global.THREE) {
                    resolve(global.THREE);
                } else {
                    threeJsLoadPromise = null;
                    reject(new Error("three.js loaded without THREE global"));
                }
            };
            script.onerror = function () {
                threeJsLoadPromise = null;
                if (script.parentNode) {
                    script.parentNode.removeChild(script);
                }
                reject(new Error("Failed to load three.js"));
            };
            (document.head || document.documentElement).appendChild(script);
        });
        return threeJsLoadPromise;
    }

    function defaultPolygon(sceneSize) {
        var size = sceneSize > 0 ? sceneSize : 1;
        var half = size * 0.35;
        // CCW in XZ when viewed from +Y so the floor normal faces up.
        return [
            { x: -half, z: -half },
            { x: -half, z: half },
            { x: half, z: half },
            { x: half, z: -half }
        ];
    }

    function polygonFromWalls(walls) {
        if (!walls || walls.length < 3) {
            return null;
        }
        var points = [];
        var index;
        for (index = 0; index < walls.length; index++) {
            points.push({ x: walls[index].x1, z: walls[index].z1 });
        }
        return points;
    }

    function defaultFloorRect(sceneSize) {
        return {
            y: 0,
            height: 0.3,
            color: DEFAULT_FLOOR_COLOR,
            // Floor plate only — walls/windows/doors live on the scene model.
            polygon: defaultPolygon(sceneSize)
        };
    }

    function normalizeFloor(floor, sceneSize) {
        if (!floor || typeof floor !== "object") {
            return defaultFloorRect(sceneSize);
        }
        if (!floor.color || typeof floor.color !== "string") {
            floor.color = DEFAULT_FLOOR_COLOR;
        }
        // Keep every floor on the same ground plane.
        floor.y = 0;
        if (!Array.isArray(floor.polygon)) {
            floor.polygon = [];
        }
        // Never rebuild a floor plate from walls — they are independent objects.
        return floor;
    }

    function hoistLegacyFloorGeometry(model) {
        if (!model || typeof model !== "object") {
            return;
        }
        if (!Array.isArray(model.walls)) {
            model.walls = [];
        }
        if (!Array.isArray(model.windows)) {
            model.windows = [];
        }
        if (!Array.isArray(model.doors)) {
            model.doors = [];
        }
        (model.floors || []).forEach(function (floor) {
            if (!floor || typeof floor !== "object") {
                return;
            }
            var wallBase = model.walls.length;
            if (Array.isArray(floor.walls) && floor.walls.length) {
                floor.walls.forEach(function (seg) {
                    model.walls.push(seg);
                });
            }
            ["windows", "doors"].forEach(function (listName) {
                if (!Array.isArray(floor[listName]) || !floor[listName].length) {
                    return;
                }
                floor[listName].forEach(function (opening) {
                    if (opening && typeof opening.wallIndex === "number") {
                        opening.wallIndex += wallBase;
                    }
                    model[listName].push(opening);
                });
            });
            // Drop nested copies so floors never own wall geometry again.
            delete floor.walls;
            delete floor.windows;
            delete floor.doors;
        });
    }

    function normalizeSceneModel(model) {
        var sceneSize = (model && model.sceneSize) || 1;
        if (!model.floors || !model.floors.length) {
            model.floors = [defaultFloorRect(sceneSize)];
        }
        hoistLegacyFloorGeometry(model);
        model.floors = model.floors.map(function (floor) {
            return normalizeFloor(floor, sceneSize);
        });
        syncModelOpenings(model);
        return model;
    }

    function pointsNearEqual(ax, az, bx, bz, epsilon) {
        var eps = epsilon == null ? 1e-6 : epsilon;
        var dx = ax - bx;
        var dz = az - bz;
        return (dx * dx + dz * dz) <= (eps * eps);
    }

    function hexToRgb(hex) {
        var value = String(hex || DEFAULT_FLOOR_COLOR).replace("#", "");
        if (value.length === 3) {
            value = value[0] + value[0] + value[1] + value[1] + value[2] + value[2];
        }
        if (value.length !== 6) {
            return { r: 106, g: 143, b: 173 };
        }
        var r = parseInt(value.slice(0, 2), 16);
        var g = parseInt(value.slice(2, 4), 16);
        var b = parseInt(value.slice(4, 6), 16);
        if (isNaN(r) || isNaN(g) || isNaN(b)) {
            return { r: 106, g: 143, b: 173 };
        }
        return { r: r, g: g, b: b };
    }

    function hexToRgba(hex, alpha) {
        var rgb = hexToRgb(hex);
        return "rgba(" + rgb.r + "," + rgb.g + "," + rgb.b + "," + alpha + ")";
    }

    function shadeRgba(hex, shade, alpha) {
        var rgb = hexToRgb(hex);
        var factor = Math.max(0.15, Math.min(1.2, shade));
        return "rgba("
            + Math.round(Math.min(255, rgb.r * factor)) + ","
            + Math.round(Math.min(255, rgb.g * factor)) + ","
            + Math.round(Math.min(255, rgb.b * factor)) + ","
            + alpha + ")";
    }

    function moveSharedWallCorner(walls, fromX, fromZ, toX, toZ) {
        if (!Array.isArray(walls)) {
            return;
        }
        var epsilon = 1e-5;
        walls.forEach(function (seg) {
            if (pointsNearEqual(seg.x1, seg.z1, fromX, fromZ, epsilon)) {
                seg.x1 = toX;
                seg.z1 = toZ;
            }
            if (pointsNearEqual(seg.x2, seg.z2, fromX, fromZ, epsilon)) {
                seg.x2 = toX;
                seg.z2 = toZ;
            }
        });
    }

    function bindOpeningToNearestWall(opening, walls) {
        if (!opening || !walls || !walls.length) {
            return;
        }
        if (typeof opening.wallIndex === "number"
            && walls[opening.wallIndex]
            && opening.t0 != null
            && opening.t1 != null) {
            return;
        }
        var bestIndex = -1;
        var bestDist = 1e9;
        var bestT0 = 0;
        var bestT1 = 1;
        walls.forEach(function (wall, index) {
            var dist = Math.max(
                distPointToSeg(opening.x1, opening.z1, wall),
                distPointToSeg(opening.x2, opening.z2, wall)
            );
            if (dist < bestDist) {
                bestDist = dist;
                bestIndex = index;
                var a = projectOnSeg(opening.x1, opening.z1, wall);
                var b = projectOnSeg(opening.x2, opening.z2, wall);
                bestT0 = Math.min(a.t, b.t);
                bestT1 = Math.max(a.t, b.t);
            }
        });
        if (bestIndex < 0) {
            return;
        }
        if (bestT1 - bestT0 < 0.02) {
            bestT0 = Math.max(0, Math.min(0.4, bestT0));
            bestT1 = Math.min(1, bestT0 + 0.2);
        }
        opening.wallIndex = bestIndex;
        opening.t0 = bestT0;
        opening.t1 = bestT1;
    }

    function syncOpeningGeometry(opening, walls) {
        if (!opening || !walls) {
            return;
        }
        bindOpeningToNearestWall(opening, walls);
        var wall = walls[opening.wallIndex];
        if (!wall || opening.t0 == null || opening.t1 == null) {
            return;
        }
        var a = pointOnWall(wall, opening.t0);
        var b = pointOnWall(wall, opening.t1);
        opening.x1 = a.x;
        opening.z1 = a.z;
        opening.x2 = b.x;
        opening.z2 = b.z;
    }

    function syncModelOpenings(model) {
        if (!model) {
            return;
        }
        var walls = model.walls || [];
        ["windows", "doors"].forEach(function (listName) {
            (model[listName] || []).forEach(function (opening) {
                syncOpeningGeometry(opening, walls);
            });
        });
    }

    function axisLockPoint(origin, point, shiftHeld) {
        if (!shiftHeld || !origin || !point) {
            return point;
        }
        var dx = Math.abs(point.x - origin.x);
        var dz = Math.abs(point.z - origin.z);
        if (dx >= dz) {
            return { x: point.x, z: origin.z };
        }
        return { x: origin.x, z: point.z };
    }

    // Floor Shift: lock the idle axis to the current adjacent vertex (not drag-start of this corner).
    function resolveFloorPolygonShift(polygon, index, axisOrigin, raw) {
        if (!polygon || !raw || index == null || index < 0 || index >= polygon.length) {
            return raw;
        }
        var count = polygon.length;
        if (count < 2) {
            return raw;
        }
        var prev = polygon[(index - 1 + count) % count];
        var next = polygon[(index + 1) % count];
        var ox = axisOrigin ? axisOrigin.x : polygon[index].x;
        var oz = axisOrigin ? axisOrigin.z : polygon[index].z;
        var dx = Math.abs(raw.x - ox);
        var dz = Math.abs(raw.z - oz);
        if (dx >= dz) {
            var lockZ = Math.abs(prev.z - oz) <= Math.abs(next.z - oz) ? prev.z : next.z;
            return { x: raw.x, z: lockZ };
        }
        var lockX = Math.abs(prev.x - ox) <= Math.abs(next.x - ox) ? prev.x : next.x;
        return { x: lockX, z: raw.z };
    }

    function pointInPolygon(point, polygon) {
        if (!point || !polygon || polygon.length < 3) {
            return false;
        }
        var inside = false;
        var index;
        for (index = 0; index < polygon.length; index++) {
            var a = polygon[index];
            var b = polygon[(index + 1) % polygon.length];
            var crosses = ((a.z > point.z) !== (b.z > point.z))
                && (point.x < (b.x - a.x) * (point.z - a.z) / ((b.z - a.z) || 1e-12) + a.x);
            if (crosses) {
                inside = !inside;
            }
        }
        return inside;
    }

    function nearestFloorCorner(floor, point, threshold) {
        if (!floor || !point) {
            return null;
        }
        var best = null;
        var bestDist = threshold;
        (floor.polygon || []).forEach(function (corner) {
            var dx = corner.x - point.x;
            var dz = corner.z - point.z;
            var dist = Math.sqrt(dx * dx + dz * dz);
            if (dist < bestDist) {
                bestDist = dist;
                best = { x: corner.x, z: corner.z };
            }
        });
        return best;
    }

    function resolveShiftDragPoint(model, floor, handle, origin, raw, shiftHeld, sceneSize) {
        if (!shiftHeld || !raw) {
            return raw;
        }
        // Walls + Shift: snap to nearest floor corner; else keep the wall horizontal/vertical.
        if (handle && handle.list === "walls") {
            var floorSnap = nearestFloorCorner(
                floor,
                raw,
                Math.max(0.02, (sceneSize || 1) * 0.045)
            );
            if (floorSnap) {
                return floorSnap;
            }
            var seg = model && model.walls && model.walls[handle.index];
            if (seg) {
                var fixed = handle.end === 1
                    ? { x: seg.x2, z: seg.z2 }
                    : { x: seg.x1, z: seg.z1 };
                return axisLockPoint(fixed, raw, true);
            }
            return raw;
        }
        // Floor polygon corners: Shift locks to current adjacent vertices.
        if (handle && handle.list === "polygon" && floor && Array.isArray(floor.polygon)) {
            return resolveFloorPolygonShift(floor.polygon, handle.index, origin, raw);
        }
        // Openings: Shift locks horizontal/vertical motion from drag origin.
        return axisLockPoint(origin, raw, true);
    }

    function smoothToward(current, target, factor) {
        var t = factor == null ? 0.55 : factor;
        if (t < 0) {
            t = 0;
        } else if (t > 1) {
            t = 1;
        }
        return {
            x: current.x + (target.x - current.x) * t,
            z: current.z + (target.z - current.z) * t
        };
    }

    function defaultModel() {
        return {
            sceneSize: 1.0,
            wallHeight: DEFAULT_WALL_HEIGHT,
            wallThickness: DEFAULT_WALL_THICKNESS,
            viewZoom: 1,
            yaw: 0.85,
            pitch: 0.62,
            perspective: true,
            // Free-fly eye used by View scene / bake (orbit seed until WASD moves it).
            eyeX: null,
            eyeY: null,
            eyeZ: null,
            // Walls / openings are scene-level — independent of floor plates.
            walls: [],
            windows: [],
            doors: [],
            floors: [defaultFloorRect(1.0)]
        };
    }

    function polygonsNearlyEqual(left, right, epsilon) {
        var eps = epsilon == null ? 1e-5 : epsilon;
        if (!left || !right || left.length !== right.length) {
            return false;
        }
        var index;
        for (index = 0; index < left.length; index++) {
            if (Math.abs((left[index].x || 0) - (right[index].x || 0)) > eps
                || Math.abs((left[index].z || 0) - (right[index].z || 0)) > eps) {
                return false;
            }
        }
        return true;
    }

    // Empty = default single floor plate and no walls/openings (import allowed).
    function isSceneModelEmpty(model) {
        if (!model || typeof model !== "object") {
            return true;
        }
        if ((model.walls || []).length
            || (model.windows || []).length
            || (model.doors || []).length) {
            return false;
        }
        var floors = model.floors || [];
        if (floors.length === 0) {
            return true;
        }
        if (floors.length !== 1) {
            return false;
        }
        return polygonsNearlyEqual(
            floors[0].polygon || [],
            defaultPolygon(model.sceneSize || 1)
        );
    }

    // Plan + camera only (no walls/openings) for "export scheme".
    function schemeModelFrom(model) {
        var source = model && model.floors ? cloneJson(model) : defaultModel();
        return {
            sceneSize: source.sceneSize || 1,
            wallHeight: source.wallHeight || DEFAULT_WALL_HEIGHT,
            wallThickness: source.wallThickness || DEFAULT_WALL_THICKNESS,
            viewZoom: source.viewZoom || 1,
            yaw: source.yaw,
            pitch: source.pitch,
            perspective: true,
            eyeX: source.eyeX,
            eyeY: source.eyeY,
            eyeZ: source.eyeZ,
            walls: [],
            windows: [],
            doors: [],
            floors: source.floors || [defaultFloorRect(source.sceneSize || 1)]
        };
    }

    function planScaleFor(sceneSize, editZoom) {
        var size = sceneSize > 0 ? sceneSize : 1;
        var zoom = Math.max(0.35, editZoom || 1);
        return (PLAN_BASE_PX * zoom) / size;
    }

    function orbitEyeFromAngles(model, yaw, pitch, viewZoom) {
        var bounds = sceneBounds(model);
        var cx = (bounds.minX + bounds.maxX) * 0.5;
        var cy = (bounds.minY + bounds.maxY) * 0.5;
        var cz = (bounds.minZ + bounds.maxZ) * 0.5;
        var radius = Math.max(
            bounds.maxX - bounds.minX,
            bounds.maxY - bounds.minY,
            bounds.maxZ - bounds.minZ,
            (model.sceneSize || 1) * 0.4
        ) * 0.5;
        var baseDist = Math.max(1.2, radius * 2.35);
        var distance = baseDist / Math.max(0.35, viewZoom || 1);
        var cp = Math.cos(pitch);
        var sp = Math.sin(pitch);
        var sy = Math.sin(yaw);
        var cyaw = Math.cos(yaw);
        return {
            x: cx + distance * sy * cp,
            y: cy + distance * sp,
            z: cz + distance * cyaw * cp,
            targetX: cx,
            targetY: cy,
            targetZ: cz
        };
    }

    function lookForwardFromAngles(yaw, pitch) {
        var cp = Math.cos(pitch);
        return {
            x: -Math.sin(yaw) * cp,
            y: -Math.sin(pitch),
            z: -Math.cos(yaw) * cp
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

    function sceneBounds(model) {
        var minX = 0;
        var maxX = 0;
        var minY = 0;
        var maxY = 0;
        var minZ = 0;
        var maxZ = 0;
        var hasPoint = false;
        function includePoint(x, y, z) {
            if (!hasPoint) {
                minX = maxX = x;
                minY = maxY = y;
                minZ = maxZ = z;
                hasPoint = true;
                return;
            }
            if (x < minX) { minX = x; }
            if (x > maxX) { maxX = x; }
            if (y < minY) { minY = y; }
            if (y > maxY) { maxY = y; }
            if (z < minZ) { minZ = z; }
            if (z > maxZ) { maxZ = z; }
        }
        var wallHeight = model.wallHeight || DEFAULT_WALL_HEIGHT;
        (model.floors || []).forEach(function (floor) {
            var baseY = floor.y || 0;
            (floor.polygon || []).forEach(function (corner) {
                includePoint(corner.x, baseY, corner.z);
                includePoint(corner.x, baseY + wallHeight, corner.z);
            });
        });
        ["walls", "windows", "doors"].forEach(function (listName) {
            (model[listName] || []).forEach(function (seg) {
                includePoint(seg.x1, 0, seg.z1);
                includePoint(seg.x2, 0, seg.z2);
                includePoint(seg.x1, wallHeight, seg.z1);
                includePoint(seg.x2, wallHeight, seg.z2);
            });
        });
        if (!hasPoint) {
            var half = (model.sceneSize || 1) * 0.35;
            return { minX: -half, maxX: half, minY: 0, maxY: wallHeight, minZ: -half, maxZ: half };
        }
        return { minX: minX, maxX: maxX, minY: minY, maxY: maxY, minZ: minZ, maxZ: maxZ };
    }

    function projectOrbit(point, yaw, pitch, distance, width, height, pixelsPerUnit, perspective) {
        var cy = Math.cos(yaw);
        var sy = Math.sin(yaw);
        var cp = Math.cos(pitch);
        var sp = Math.sin(pitch);
        var x = point.x;
        var y = point.y;
        var z = point.z;
        var rx = x * cy + z * sy;
        var rz = -x * sy + z * cy;
        var ry = y * cp + rz * sp;
        var rz2 = -y * sp + rz * cp;
        var depth = distance + rz2;
        if (depth < 0.05) {
            depth = 0.05;
        }
        var base = pixelsPerUnit > 0 ? pixelsPerUnit : Math.min(width, height) * 0.42;
        var scale = perspective === false ? (base / Math.max(distance, 0.05)) : (base / depth);
        return {
            x: width * 0.5 + rx * scale,
            y: height * 0.58 - ry * scale,
            depth: depth
        };
    }

    function viewProjectionParams(model, width, height, yaw, pitch) {
        var bounds = sceneBounds(model);
        var cx = (bounds.minX + bounds.maxX) * 0.5;
        var cy = (bounds.minY + bounds.maxY) * 0.5;
        var cz = (bounds.minZ + bounds.maxZ) * 0.5;
        var radius = Math.max(
            bounds.maxX - bounds.minX,
            bounds.maxY - bounds.minY,
            bounds.maxZ - bounds.minZ,
            (model.sceneSize || 1) * 0.4
        ) * 0.5;
        var distance = Math.max(1.2, radius * 2.35);
        var perspective = model.perspective !== false;
        var sample = [
            { x: bounds.minX - cx, y: bounds.minY - cy, z: bounds.minZ - cz },
            { x: bounds.maxX - cx, y: bounds.minY - cy, z: bounds.minZ - cz },
            { x: bounds.minX - cx, y: bounds.maxY - cy, z: bounds.minZ - cz },
            { x: bounds.maxX - cx, y: bounds.maxY - cy, z: bounds.minZ - cz },
            { x: bounds.minX - cx, y: bounds.minY - cy, z: bounds.maxZ - cz },
            { x: bounds.maxX - cx, y: bounds.minY - cy, z: bounds.maxZ - cz },
            { x: bounds.minX - cx, y: bounds.maxY - cy, z: bounds.maxZ - cz },
            { x: bounds.maxX - cx, y: bounds.maxY - cy, z: bounds.maxZ - cz }
        ];
        var maxSpan = 0.001;
        sample.forEach(function (point) {
            var projected = projectOrbit(
                point, yaw, pitch, distance, width, height, 1, perspective
            );
            var dx = projected.x - width * 0.5;
            var dy = projected.y - height * 0.58;
            var span = Math.sqrt(dx * dx + dy * dy);
            if (span > maxSpan) {
                maxSpan = span;
            }
        });
        var targetRadiusPx = Math.min(width, height) * 0.38;
        return {
            centerX: cx,
            centerY: cy,
            centerZ: cz,
            distance: distance,
            pixelsPerUnit: targetRadiusPx / maxSpan,
            perspective: perspective
        };
    }

    function averageDepth(projected) {
        var sum = 0;
        var index;
        for (index = 0; index < projected.length; index++) {
            sum += projected[index].depth;
        }
        return sum / Math.max(1, projected.length);
    }

    function paintFaces(ctx, faces, yaw, pitch, distance, width, height, pixelsPerUnit, center, perspective) {
        var light = normalize3({ x: 0.35, y: 0.9, z: 0.25 });
        var shift = center || { x: 0, y: 0, z: 0 };
        var usePerspective = perspective !== false;
        var prepared = faces.map(function (face) {
            var projected = face.points.map(function (point) {
                return projectOrbit({
                    x: point.x - shift.x,
                    y: point.y - shift.y,
                    z: point.z - shift.z
                }, yaw, pitch, distance, width, height, pixelsPerUnit, usePerspective);
            });
            var facingScreen = 0;
            if (projected.length >= 3) {
                facingScreen = (projected[1].x - projected[0].x) * (projected[2].y - projected[0].y)
                    - (projected[1].y - projected[0].y) * (projected[2].x - projected[0].x);
            }
            if (!face.forceDraw && facingScreen > 0) {
                return null;
            }
            var normal = faceNormal(face.points);
            var facing = Math.max(0.2, Math.abs(
                normal.x * light.x + normal.y * light.y + normal.z * light.z
            ));
            var shade = 0.45 + 0.55 * facing;
            return {
                projected: projected,
                depth: averageDepth(projected),
                layer: face.layer == null ? 1 : face.layer,
                fill: shadeRgba(face.color, shade, face.alpha == null ? 1 : face.alpha),
                stroke: shadeRgba(face.color, shade * 0.7, 0.65),
                noStroke: !!face.noStroke
            };
        }).filter(function (face) {
            return !!face;
        });
        prepared.sort(function (left, right) {
            if (left.layer !== right.layer) {
                return left.layer - right.layer;
            }
            return right.depth - left.depth;
        });
        prepared.forEach(function (face) {
            if (!face.projected.length) {
                return;
            }
            ctx.beginPath();
            face.projected.forEach(function (point, index) {
                if (index === 0) {
                    ctx.moveTo(point.x, point.y);
                } else {
                    ctx.lineTo(point.x, point.y);
                }
            });
            ctx.closePath();
            ctx.fillStyle = face.fill;
            ctx.fill();
            if (!face.noStroke) {
                ctx.strokeStyle = face.stroke;
                ctx.lineWidth = 1;
                ctx.stroke();
            }
        });
    }

    function cross(ax, ay, az, bx, by, bz) {
        return {
            x: ay * bz - az * by,
            y: az * bx - ax * bz,
            z: ax * by - ay * bx
        };
    }

    function normalize3(vector) {
        var len = Math.sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
        if (len < 1e-9) {
            return { x: 0, y: 1, z: 0 };
        }
        return { x: vector.x / len, y: vector.y / len, z: vector.z / len };
    }

    function faceNormal(points) {
        if (!points || points.length < 3) {
            return { x: 0, y: 1, z: 0 };
        }
        return normalize3(cross(
            points[1].x - points[0].x,
            points[1].y - points[0].y,
            points[1].z - points[0].z,
            points[2].x - points[0].x,
            points[2].y - points[0].y,
            points[2].z - points[0].z
        ));
    }

    function wallLength(seg) {
        var dx = seg.x2 - seg.x1;
        var dz = seg.z2 - seg.z1;
        return Math.sqrt(dx * dx + dz * dz);
    }

    function pointOnWall(seg, t) {
        return {
            x: seg.x1 + (seg.x2 - seg.x1) * t,
            z: seg.z1 + (seg.z2 - seg.z1) * t
        };
    }

    function endpointsNear(ax, az, bx, bz, epsilon) {
        return pointsNearEqual(ax, az, bx, bz, epsilon == null ? 1e-4 : epsilon);
    }

    function wallConnectedAt(walls, wallIndex, end, epsilon) {
        var wall = walls[wallIndex];
        if (!wall) {
            return false;
        }
        var x = end === 1 ? wall.x1 : wall.x2;
        var z = end === 1 ? wall.z1 : wall.z2;
        var index;
        for (index = 0; index < walls.length; index++) {
            if (index === wallIndex) {
                continue;
            }
            var other = walls[index];
            if (endpointsNear(x, z, other.x1, other.z1, epsilon)
                || endpointsNear(x, z, other.x2, other.z2, epsilon)) {
                return true;
            }
        }
        return false;
    }

    // Square post filling the gap left when connected walls are trimmed by half thickness.
    function cornerPostFaces(x, z, baseY, wallHeight, thickness, colorHex) {
        var half = Math.max(0.004, thickness * 0.5);
        return wallBoxFaces(
            { x1: x - half, z1: z, x2: x + half, z2: z },
            baseY,
            wallHeight,
            thickness,
            colorHex,
            1,
            { trimStart: 0, trimEnd: 0, skipStartCap: false, skipEndCap: false }
        );
    }

    function pushWallFace(faces, points, colorHex, options) {
        options = options || {};
        faces.push({
            points: points,
            color: colorHex,
            alpha: 1,
            forceDraw: options.forceDraw !== false,
            noStroke: !!options.noStroke,
            layer: options.layer == null ? 1 : options.layer
        });
    }

    // Thick wall box. forceDraw keeps both faces + top cap visible (real thickness).
    function wallBoxFaces(seg, baseY, wallHeight, thickness, colorHex, alpha, options) {
        var faces = [];
        options = options || {};
        var dx = seg.x2 - seg.x1;
        var dz = seg.z2 - seg.z1;
        var len = Math.sqrt(dx * dx + dz * dz);
        if (len < 1e-9) {
            return faces;
        }
        var ux = dx / len;
        var uz = dz / len;
        var nx = -uz;
        var nz = ux;
        var half = Math.max(0.0005, thickness * 0.5);
        var trimStart = options.trimStart || 0;
        var trimEnd = options.trimEnd || 0;
        if (trimStart + trimEnd > len * 0.85) {
            var scale = (len * 0.85) / (trimStart + trimEnd);
            trimStart *= scale;
            trimEnd *= scale;
        }
        var x1 = seg.x1 + ux * trimStart;
        var z1 = seg.z1 + uz * trimStart;
        var x2 = seg.x2 - ux * trimEnd;
        var z2 = seg.z2 - uz * trimEnd;
        var y0 = baseY;
        var y1 = baseY + wallHeight;
        var aOut = { x: x1 + nx * half, y: y0, z: z1 + nz * half };
        var bOut = { x: x2 + nx * half, y: y0, z: z2 + nz * half };
        var bIn = { x: x2 - nx * half, y: y0, z: z2 - nz * half };
        var aIn = { x: x1 - nx * half, y: y0, z: z1 - nz * half };
        var aOutTop = { x: aOut.x, y: y1, z: aOut.z };
        var bOutTop = { x: bOut.x, y: y1, z: bOut.z };
        var bInTop = { x: bIn.x, y: y1, z: bIn.z };
        var aInTop = { x: aIn.x, y: y1, z: aIn.z };
        var layer = options.layer == null ? 1 : options.layer;
        var faceOpts = { noStroke: !!options.noStroke, layer: layer };
        pushWallFace(faces, [aOut, bOut, bOutTop, aOutTop], colorHex, faceOpts);
        pushWallFace(faces, [bIn, aIn, aInTop, bInTop], colorHex, faceOpts);
        if (!options.skipStartCap) {
            pushWallFace(faces, [aIn, aOut, aOutTop, aInTop], colorHex, faceOpts);
        }
        if (!options.skipEndCap) {
            pushWallFace(faces, [bOut, bIn, bInTop, bOutTop], colorHex, faceOpts);
        }
        pushWallFace(faces, [aOutTop, bOutTop, bInTop, aInTop], colorHex, faceOpts);
        return faces;
    }

    function loopSignedArea(points) {
        var area = 0;
        var index;
        for (index = 0; index < points.length; index++) {
            var a = points[index];
            var b = points[(index + 1) % points.length];
            area += a.x * b.z - b.x * a.z;
        }
        return area * 0.5;
    }

    function miterRibbon(loop, half) {
        var count = loop.length;
        var ccw = loopSignedArea(loop) > 0;
        var outer = [];
        var inner = [];
        var index;
        for (index = 0; index < count; index++) {
            var prev = loop[(index - 1 + count) % count];
            var cur = loop[index];
            var next = loop[(index + 1) % count];
            var e1x = cur.x - prev.x;
            var e1z = cur.z - prev.z;
            var e2x = next.x - cur.x;
            var e2z = next.z - cur.z;
            var len1 = Math.sqrt(e1x * e1x + e1z * e1z);
            var len2 = Math.sqrt(e2x * e2x + e2z * e2z);
            if (len1 < 1e-9 || len2 < 1e-9) {
                outer.push({ x: cur.x, z: cur.z });
                inner.push({ x: cur.x, z: cur.z });
                continue;
            }
            e1x /= len1;
            e1z /= len1;
            e2x /= len2;
            e2z /= len2;
            var n1x = -e1z;
            var n1z = e1x;
            var n2x = -e2z;
            var n2z = e2x;
            if (!ccw) {
                n1x = -n1x;
                n1z = -n1z;
                n2x = -n2x;
                n2z = -n2z;
            }
            // n* points inward for a CCW (or flipped CW) loop.
            var mx = n1x + n2x;
            var mz = n1z + n2z;
            var mlen = Math.sqrt(mx * mx + mz * mz);
            if (mlen < 1e-6) {
                mx = n1x;
                mz = n1z;
                mlen = 1;
            }
            mx /= mlen;
            mz /= mlen;
            var den = mx * n1x + mz * n1z;
            if (Math.abs(den) < 0.25) {
                den = den < 0 ? -0.25 : 0.25;
            }
            var m = half / den;
            if (m > half * 2.5) {
                m = half * 2.5;
            } else if (m < -half * 2.5) {
                m = -half * 2.5;
            }
            inner.push({ x: cur.x + mx * m, z: cur.z + mz * m });
            outer.push({ x: cur.x - mx * m, z: cur.z - mz * m });
        }
        return { outer: outer, inner: inner };
    }

    function ribbonSegmentFaces(outerA, outerB, innerA, innerB, y0, y1, colorHex, options) {
        var faces = [];
        var noStroke = !!(options && options.noStroke);
        var layer = options && options.layer != null ? options.layer : 1;
        var faceOpts = { noStroke: noStroke, layer: layer };
        var oA0 = { x: outerA.x, y: y0, z: outerA.z };
        var oB0 = { x: outerB.x, y: y0, z: outerB.z };
        var iB0 = { x: innerB.x, y: y0, z: innerB.z };
        var iA0 = { x: innerA.x, y: y0, z: innerA.z };
        var oA1 = { x: outerA.x, y: y1, z: outerA.z };
        var oB1 = { x: outerB.x, y: y1, z: outerB.z };
        var iB1 = { x: innerB.x, y: y1, z: innerB.z };
        var iA1 = { x: innerA.x, y: y1, z: innerA.z };
        pushWallFace(faces, [oA0, oB0, oB1, oA1], colorHex, faceOpts);
        pushWallFace(faces, [iB0, iA0, iA1, iB1], colorHex, faceOpts);
        pushWallFace(faces, [oA1, oB1, iB1, iA1], colorHex, faceOpts);
        // Thickness reveals where walls are slid apart (door openings).
        if (options && options.capStart) {
            pushWallFace(faces, [iA0, oA0, oA1, iA1], colorHex, faceOpts);
        }
        if (options && options.capEnd) {
            pushWallFace(faces, [oB0, iB0, iB1, oB1], colorHex, faceOpts);
        }
        return faces;
    }

    function lerp2(a, b, t) {
        return {
            x: a.x + (b.x - a.x) * t,
            z: a.z + (b.z - a.z) * t
        };
    }

    function closedLoopWallFaces(loop, openings, baseY, wallHeight, thickness, wallColor) {
        var faces = [];
        var half = Math.max(0.0005, thickness * 0.5);
        var ribbon = miterRibbon(loop, half);
        var count = loop.length;
        var y0 = baseY;
        var y1 = baseY + wallHeight;
        var edgeIndex;
        for (edgeIndex = 0; edgeIndex < count; edgeIndex++) {
            var a = loop[edgeIndex];
            var b = loop[(edgeIndex + 1) % count];
            var center = { x1: a.x, z1: a.z, x2: b.x, z2: b.z };
            var cuts = [];
            openings.forEach(function (opening) {
                var span = projectOpeningOntoWall(center, opening.seg, Math.max(thickness * 2.5, 0.03));
                if (!span) {
                    return;
                }
                cuts.push({
                    t0: span.t0,
                    t1: span.t1,
                    color: opening.color,
                    y0: opening.y0,
                    y1: opening.y1,
                    fillPanel: !!opening.fillPanel
                });
            });
            cuts.sort(function (left, right) {
                return left.t0 - right.t0;
            });
            var oA = ribbon.outer[edgeIndex];
            var oB = ribbon.outer[(edgeIndex + 1) % count];
            var iA = ribbon.inner[edgeIndex];
            var iB = ribbon.inner[(edgeIndex + 1) % count];

            function solidRibbon(t0, t1, yy0, yy1, color, faceOptions) {
                if (t1 <= t0 + 1e-4 || yy1 <= yy0 + 1e-6) {
                    return;
                }
                faces = faces.concat(ribbonSegmentFaces(
                    lerp2(oA, oB, t0),
                    lerp2(oA, oB, t1),
                    lerp2(iA, iB, t0),
                    lerp2(iA, iB, t1),
                    yy0,
                    yy1,
                    color,
                    faceOptions
                ));
            }

            var cursor = 0;
            if (!cuts.length) {
                solidRibbon(0, 1, y0, y1, wallColor);
            } else {
                cuts.forEach(function (cut) {
                    var t0 = Math.max(cursor, cut.t0);
                    var t1 = Math.max(t0, cut.t1);
                    // Windows + doors: noStroke on abutting pieces so cut edges stay clean.
                    // Doors = walls slid apart (thickness caps, no colored jambs).
                    var abutOpts = { noStroke: true };
                    if (t0 > cursor + 1e-4) {
                        solidRibbon(cursor, t0, y0, y1, wallColor, Object.assign({
                            capEnd: !cut.fillPanel
                        }, abutOpts));
                    }
                    if (cut.y0 > y0 + 1e-4) {
                        solidRibbon(t0, t1, y0, cut.y0, wallColor, Object.assign({
                            capStart: !cut.fillPanel,
                            capEnd: !cut.fillPanel
                        }, abutOpts));
                    }
                    if (cut.y1 < y1 - 1e-4) {
                        solidRibbon(t0, t1, cut.y1, y1, wallColor, Object.assign({
                            capStart: !cut.fillPanel,
                            capEnd: !cut.fillPanel
                        }, abutOpts));
                    }
                    if (cut.fillPanel) {
                        // Window panel overlaps cut slightly; no stroke (layer above wall).
                        var padT = 0.006;
                        solidRibbon(
                            Math.max(0, t0 - padT),
                            Math.min(1, t1 + padT),
                            cut.y0,
                            cut.y1,
                            cut.color,
                            { noStroke: true, layer: 2 }
                        );
                    }
                    cursor = Math.max(cursor, t1);
                });
                if (cursor < 1 - 1e-4) {
                    var lastCut = cuts[cuts.length - 1];
                    solidRibbon(
                        cursor,
                        1,
                        y0,
                        y1,
                        wallColor,
                        Object.assign({
                            capStart: !!(lastCut && !lastCut.fillPanel)
                        }, { noStroke: true })
                    );
                }
            }
        }
        return faces;
    }

    function projectOpeningOntoWall(wall, opening, maxDist) {
        var len = wallLength(wall);
        if (len < 1e-9) {
            return null;
        }
        var a = projectOnSeg(opening.x1, opening.z1, wall);
        var b = projectOnSeg(opening.x2, opening.z2, wall);
        var distA = distPointToSeg(opening.x1, opening.z1, wall);
        var distB = distPointToSeg(opening.x2, opening.z2, wall);
        if (distA > maxDist || distB > maxDist) {
            return null;
        }
        var t0 = Math.min(a.t, b.t);
        var t1 = Math.max(a.t, b.t);
        if (t0 < 0) {
            t0 = 0;
        }
        if (t1 > 1) {
            t1 = 1;
        }
        if (t1 - t0 < 0.02) {
            return null;
        }
        return { t0: t0, t1: t1 };
    }

    function wallSlab(wall, t0, t1, baseY, height, thickness, colorHex, alpha, options) {
        if (height <= 1e-6 || t1 - t0 <= 1e-4) {
            return [];
        }
        var a = pointOnWall(wall, t0);
        var b = pointOnWall(wall, t1);
        return wallBoxFaces(
            { x1: a.x, z1: a.z, x2: b.x, z2: b.z },
            baseY,
            height,
            thickness,
            colorHex,
            alpha,
            options || {}
        );
    }

    function wallFacesWithOpenings(wall, openings, baseY, wallHeight, thickness, joinOptions) {
        var faces = [];
        var wallColor = "#cfd6e0";
        var half = Math.max(0.0005, thickness * 0.5);
        var join = joinOptions || {};
        var trimStart = join.connectedStart ? half : 0;
        var trimEnd = join.connectedEnd ? half : 0;
        var skipStartCap = !!join.connectedStart;
        var skipEndCap = !!join.connectedEnd;
        var cuts = [];
        openings.forEach(function (opening) {
            var span = projectOpeningOntoWall(wall, opening.seg, Math.max(thickness * 2.5, 0.03));
            if (!span) {
                return;
            }
            cuts.push({
                t0: span.t0,
                t1: span.t1,
                color: opening.color,
                y0: opening.y0,
                y1: opening.y1,
                fillPanel: !!opening.fillPanel
            });
        });
        cuts.sort(function (left, right) {
            return left.t0 - right.t0;
        });

        function solidSpan(t0, t1, y0, height, color, extraOptions) {
            if (t1 <= t0 || height <= 1e-6) {
                return;
            }
            var options = {
                trimStart: 0,
                trimEnd: 0,
                skipStartCap: true,
                skipEndCap: true
            };
            if (t0 <= 1e-4) {
                options.trimStart = trimStart;
                options.skipStartCap = skipStartCap;
            }
            if (t1 >= 1 - 1e-4) {
                options.trimEnd = trimEnd;
                options.skipEndCap = skipEndCap;
            }
            if (extraOptions) {
                Object.keys(extraOptions).forEach(function (key) {
                    options[key] = extraOptions[key];
                });
            }
            faces = faces.concat(wallSlab(wall, t0, t1, y0, height, thickness, color, 1, options));
        }

        var cursor = 0;
        if (!cuts.length) {
            solidSpan(0, 1, baseY, wallHeight, wallColor);
            return faces;
        }
        cuts.forEach(function (cut) {
            var t0 = Math.max(cursor, cut.t0);
            var t1 = Math.max(t0, cut.t1);
            // Doors: walls slid apart — show thickness caps at the opening, no jamb strips.
            var doorSlide = !cut.fillPanel;
            var abutOpts = { noStroke: true };
            if (t0 > cursor + 1e-4) {
                solidSpan(cursor, t0, baseY, wallHeight, wallColor, Object.assign({
                    skipEndCap: !doorSlide
                }, abutOpts));
            }
            if (cut.y0 > baseY + 1e-4) {
                solidSpan(t0, t1, baseY, cut.y0 - baseY, wallColor, Object.assign({
                    skipStartCap: !doorSlide,
                    skipEndCap: !doorSlide
                }, abutOpts));
            }
            if (cut.y1 < baseY + wallHeight - 1e-4) {
                solidSpan(t0, t1, cut.y1, (baseY + wallHeight) - cut.y1, wallColor, Object.assign({
                    skipStartCap: !doorSlide,
                    skipEndCap: !doorSlide
                }, abutOpts));
            }
            if (cut.fillPanel) {
                var padT = 0.006;
                solidSpan(
                    Math.max(0, t0 - padT),
                    Math.min(1, t1 + padT),
                    cut.y0,
                    cut.y1 - cut.y0,
                    cut.color,
                    { noStroke: true, skipStartCap: true, skipEndCap: true, layer: 2 }
                );
            }
            cursor = Math.max(cursor, t1);
        });
        if (cursor < 1 - 1e-4) {
            var lastCut = cuts[cuts.length - 1];
            var lastDoor = !!(lastCut && !lastCut.fillPanel);
            solidSpan(
                cursor,
                1,
                baseY,
                wallHeight,
                wallColor,
                Object.assign({
                    skipStartCap: !lastDoor
                }, { noStroke: true })
            );
        }
        return faces;
    }

    function closedLoopFromWalls(walls, joinEpsilon) {
        if (!walls || walls.length < 3) {
            return null;
        }
        var epsilon = joinEpsilon == null ? 1e-3 : joinEpsilon;
        var used = walls.map(function () { return false; });
        var points = [{ x: walls[0].x1, z: walls[0].z1 }, { x: walls[0].x2, z: walls[0].z2 }];
        used[0] = true;
        var guard;
        for (guard = 0; guard < walls.length + 2; guard++) {
            var tip = points[points.length - 1];
            var found = -1;
            var reverse = false;
            var index;
            for (index = 0; index < walls.length; index++) {
                if (used[index]) {
                    continue;
                }
                if (endpointsNear(tip.x, tip.z, walls[index].x1, walls[index].z1, epsilon)) {
                    found = index;
                    reverse = false;
                    break;
                }
                if (endpointsNear(tip.x, tip.z, walls[index].x2, walls[index].z2, epsilon)) {
                    found = index;
                    reverse = true;
                    break;
                }
            }
            if (found < 0) {
                break;
            }
            used[found] = true;
            var next = reverse
                ? { x: walls[found].x1, z: walls[found].z1 }
                : { x: walls[found].x2, z: walls[found].z2 };
            points.push(next);
        }
        var allUsed = used.every(function (flag) { return flag; });
        if (!allUsed || points.length < 4) {
            return null;
        }
        var first = points[0];
        var last = points[points.length - 1];
        if (!endpointsNear(first.x, first.z, last.x, last.z, epsilon)) {
            return null;
        }
        points.pop();
        return points;
    }

    function makeFloorFace(points2d, baseY, colorHex) {
        if (!points2d || points2d.length < 3) {
            return null;
        }
        var floorPoints = points2d.map(function (corner) {
            return { x: corner.x, y: baseY + 0.001, z: corner.z };
        });
        if (faceNormal(floorPoints).y < 0) {
            floorPoints = floorPoints.slice().reverse();
        }
        return {
            points: floorPoints,
            color: colorHex || DEFAULT_FLOOR_COLOR,
            alpha: 1,
            forceDraw: true,
            layer: 0,
            // Concave multi-room plates need ear-clip; fan spills outside the outline.
            earClip: true
        };
    }

    function polygonAreaXZ(points) {
        var area = 0;
        var index;
        for (index = 0; index < points.length; index++) {
            var a = points[index];
            var b = points[(index + 1) % points.length];
            area += a.x * b.z - b.x * a.z;
        }
        return area * 0.5;
    }

    function pointInTriangleXZ(p, a, b, c) {
        var area = function (p0, p1, p2) {
            return (p1.x - p0.x) * (p2.z - p0.z) - (p2.x - p0.x) * (p1.z - p0.z);
        };
        var a0 = area(p, a, b);
        var a1 = area(p, b, c);
        var a2 = area(p, c, a);
        var hasNeg = (a0 < 0) || (a1 < 0) || (a2 < 0);
        var hasPos = (a0 > 0) || (a1 > 0) || (a2 > 0);
        return !(hasNeg && hasPos);
    }

    function isEarXZ(points, prev, index, next, ccw) {
        var a = points[prev];
        var b = points[index];
        var c = points[next];
        var cross = (b.x - a.x) * (c.z - a.z) - (c.x - a.x) * (b.z - a.z);
        if (ccw ? cross <= 1e-12 : cross >= -1e-12) {
            return false;
        }
        var i;
        for (i = 0; i < points.length; i++) {
            if (i === prev || i === index || i === next) {
                continue;
            }
            if (pointInTriangleXZ(points[i], a, b, c)) {
                return false;
            }
        }
        return true;
    }

    function earClipPositionsXZ(points) {
        var positions = [];
        if (!points || points.length < 3) {
            return positions;
        }
        if (points.length === 3) {
            points.forEach(function (p) {
                positions.push(p.x, p.y, p.z);
            });
            return positions;
        }
        var work = points.map(function (p, index) {
            return { x: p.x, y: p.y, z: p.z, _i: index };
        });
        var ccw = polygonAreaXZ(work) > 0;
        var guard = 0;
        var maxGuard = work.length * work.length + 8;
        while (work.length > 3 && guard < maxGuard) {
            guard += 1;
            var earFound = false;
            var i;
            for (i = 0; i < work.length; i++) {
                var prev = (i - 1 + work.length) % work.length;
                var next = (i + 1) % work.length;
                if (!isEarXZ(work, prev, i, next, ccw)) {
                    continue;
                }
                var a = work[prev];
                var b = work[i];
                var c = work[next];
                positions.push(a.x, a.y, a.z, b.x, b.y, b.z, c.x, c.y, c.z);
                work.splice(i, 1);
                earFound = true;
                break;
            }
            if (!earFound) {
                break;
            }
        }
        if (work.length === 3) {
            positions.push(
                work[0].x, work[0].y, work[0].z,
                work[1].x, work[1].y, work[1].z,
                work[2].x, work[2].y, work[2].z
            );
        }
        return positions;
    }

    function buildSceneFaces(model) {
        var faces = [];
        var wallHeight = model.wallHeight || DEFAULT_WALL_HEIGHT;
        var thickness = model.wallThickness || DEFAULT_WALL_THICKNESS;
        var wallColor = "#cfd6e0";
        var baseY = 0;
        syncModelOpenings(model);

        // Floor plates only — never derived from walls.
        (model.floors || []).forEach(function (floor) {
            var floorFace = makeFloorFace(floor.polygon, baseY, floor.color);
            if (floorFace) {
                faces.push(floorFace);
            }
        });

        var walls = model.walls || [];
        var openings = [];
        (model.doors || []).forEach(function (seg) {
            openings.push({
                seg: seg,
                color: wallColor,
                y0: baseY,
                y1: baseY + wallHeight * 0.92,
                fillPanel: false
            });
        });
        (model.windows || []).forEach(function (seg) {
            var lift = wallHeight * 0.28;
            var panelHeight = wallHeight * 0.5;
            openings.push({
                seg: seg,
                color: "#4fc3f7",
                y0: baseY + lift,
                y1: baseY + lift + panelHeight,
                fillPanel: true
            });
        });

        var joinEps = Math.max(1e-3, thickness * 0.35);
        var loop = closedLoopFromWalls(walls, joinEps);
        if (loop) {
            faces = faces.concat(closedLoopWallFaces(
                loop, openings, baseY, wallHeight, thickness, wallColor
            ));
        } else {
            var cornerKeys = {};
            walls.forEach(function (seg, index) {
                var join = {
                    connectedStart: wallConnectedAt(walls, index, 1, joinEps),
                    connectedEnd: wallConnectedAt(walls, index, 2, joinEps)
                };
                faces = faces.concat(wallFacesWithOpenings(
                    seg, openings, baseY, wallHeight, thickness, join
                ));
                if (join.connectedStart) {
                    cornerKeys[seg.x1.toFixed(5) + "," + seg.z1.toFixed(5)] = {
                        x: seg.x1, z: seg.z1
                    };
                }
                if (join.connectedEnd) {
                    cornerKeys[seg.x2.toFixed(5) + "," + seg.z2.toFixed(5)] = {
                        x: seg.x2, z: seg.z2
                    };
                }
            });
            Object.keys(cornerKeys).forEach(function (key) {
                var corner = cornerKeys[key];
                faces = faces.concat(cornerPostFaces(
                    corner.x, corner.z, baseY, wallHeight, thickness, wallColor
                ));
            });
        }
        return faces;
    }

    function hexToThreeColorInt(hex) {
        var value = String(hex || "#ffffff").replace("#", "");
        if (value.length === 3) {
            value = value[0] + value[0] + value[1] + value[1] + value[2] + value[2];
        }
        if (value.length !== 6) {
            return 0xffffff;
        }
        var parsed = parseInt(value, 16);
        return isNaN(parsed) ? 0xffffff : parsed;
    }

    function faceToThreeGeometry(face, THREE) {
        var pts = face && face.points;
        if (!pts || pts.length < 3) {
            return null;
        }
        var positions;
        if (face.earClip) {
            positions = earClipPositionsXZ(pts);
        } else {
            positions = [];
            var index;
            for (index = 1; index < pts.length - 1; index++) {
                positions.push(pts[0].x, pts[0].y, pts[0].z);
                positions.push(pts[index].x, pts[index].y, pts[index].z);
                positions.push(pts[index + 1].x, pts[index + 1].y, pts[index + 1].z);
            }
        }
        if (!positions.length) {
            return null;
        }
        var geometry = new THREE.BufferGeometry();
        geometry.setAttribute("position", new THREE.Float32BufferAttribute(positions, 3));
        geometry.computeVertexNormals();
        return geometry;
    }

    function buildThreeSceneRoot(model, THREE) {
        var root = new THREE.Group();
        buildSceneFaces(model).forEach(function (face) {
            var geometry = faceToThreeGeometry(face, THREE);
            if (!geometry) {
                return;
            }
            var opacity = face.alpha == null ? 1 : face.alpha;
            var material = new THREE.MeshLambertMaterial({
                color: hexToThreeColorInt(face.color),
                side: THREE.DoubleSide,
                transparent: opacity < 0.999,
                opacity: opacity
            });
            root.add(new THREE.Mesh(geometry, material));
        });
        return root;
    }

    function SceneDesigner(canvas) {
        this.canvas = canvas;
        this.ctx = canvas.getContext("2d", { alpha: true });
        this.model = defaultModel();
        this.floorIndex = 0;
        this.tool = "wall";
        this.viewMode = false;
        this.yaw = 0.85;
        this.pitch = 0.62;
        this.viewZoom = 1;
        this.perspective = true;
        this.editZoom = 1;
        this.eyeX = null;
        this.eyeY = null;
        this.eyeZ = null;
        this._flyMoved = false;
        this.dragOrbit = false;
        this.dragHandle = null;
        this.dragMoved = false;
        this.dragStartClientX = 0;
        this.dragStartClientY = 0;
        this.dragCornerFrom = null;
        this.dragCornerOrigin = null;
        this.dragShiftHeld = false;
        this._keysDown = {};
        this._flyRaf = 0;
        this._flyLastTs = 0;
        this.lastDragClientX = 0;
        this.lastDragClientY = 0;
        this.lastX = 0;
        this.lastY = 0;
        this.onChange = null;
        this.selection = null;
        this._three = null;
        this._threeUnavailable = false;
        this.bindEvents();
        this.resize();
    }

    SceneDesigner.prototype.bindEvents = function () {
        var self = this;
        // Drop any prior designer still wired to this canvas (orphan after null-without-dispose).
        if (this.canvas && this.canvas.__sceneDesigner && this.canvas.__sceneDesigner !== this) {
            this.canvas.__sceneDesigner.dispose();
        }
        if (this.canvas) {
            this.canvas.__sceneDesigner = this;
        }
        this._onCanvasMouseDown = function (event) {
            event.preventDefault();
            event.stopPropagation();
            self.onPointerDown(event);
        };
        this._onCanvasMouseMove = function (event) {
            self.onPointerMove(event);
        };
        this._onWindowMouseMove = function (event) {
            if (!self.dragOrbit && !self.dragHandle) {
                return;
            }
            self.onPointerMove(event);
        };
        this._onWindowMouseUp = function (event) {
            self.dragOrbit = false;
            if (self.dragHandle) {
                self.finishHandleDrag(event);
            }
        };
        this._onWindowKeyDown = function (event) {
            if (event.key === "Shift" && self.dragHandle) {
                self.dragShiftHeld = true;
                self.applyHandleDragAtClient(self.lastDragClientX, self.lastDragClientY, true);
                return;
            }
            var active = document.activeElement;
            var tag = active && active.tagName ? active.tagName.toLowerCase() : "";
            var typing = tag === "input" || tag === "textarea" || tag === "select"
                || (active && active.isContentEditable);
            if (!typing && self.viewMode && self.canvas && self.canvas.offsetParent !== null) {
                var flyKey = String(event.key || "").toLowerCase();
                if (flyKey === "w" || flyKey === "a" || flyKey === "s" || flyKey === "d") {
                    event.preventDefault();
                    self._keysDown[flyKey] = true;
                    self.ensureEyeFromOrbit();
                    self.startFlyLoop();
                    return;
                }
            }
            if (event.key === "Delete" || event.key === "Del") {
                if (typing) {
                    return;
                }
                if (self.canvas && self.canvas.offsetParent === null) {
                    return;
                }
                event.preventDefault();
                self.removeSelected();
            }
        };
        this._onWindowKeyUp = function (event) {
            var flyKey = String(event.key || "").toLowerCase();
            if (flyKey === "w" || flyKey === "a" || flyKey === "s" || flyKey === "d") {
                self._keysDown[flyKey] = false;
            }
            if (event.key !== "Shift" || !self.dragHandle) {
                return;
            }
            self.dragShiftHeld = false;
            self.applyHandleDragAtClient(self.lastDragClientX, self.lastDragClientY, false);
        };
        this._onCanvasDblClick = function (event) {
            self.onDoubleClick(event);
        };
        this._onCanvasWheel = function (event) {
            event.preventDefault();
            var factor = Math.exp(-event.deltaY * 0.0012);
            if (self.viewMode) {
                // Wheel always zooms the orbit around scene center (same as room scaling).
                self._flyMoved = false;
                self.viewZoom = Math.max(0.35, Math.min(3.5, self.viewZoom * factor));
                self.model.viewZoom = self.viewZoom;
                self.eyeX = null;
                self.eyeY = null;
                self.eyeZ = null;
                self.ensureEyeFromOrbit();
                if (typeof self.onChange === "function") {
                    self.onChange();
                }
            } else {
                // Plan/edit zoom only — not persisted (3D viewZoom is separate).
                self.editZoom = Math.max(0.35, Math.min(6, self.editZoom * factor));
            }
            self.draw();
        };
        this.canvas.addEventListener("mousedown", this._onCanvasMouseDown);
        this.canvas.addEventListener("mousemove", this._onCanvasMouseMove);
        this.canvas.addEventListener("dblclick", this._onCanvasDblClick);
        this.canvas.addEventListener("wheel", this._onCanvasWheel, { passive: false });
        window.addEventListener("mousemove", this._onWindowMouseMove);
        window.addEventListener("mouseup", this._onWindowMouseUp);
        window.addEventListener("keydown", this._onWindowKeyDown);
        window.addEventListener("keyup", this._onWindowKeyUp);
        this._onWindowResize = function () {
            self.resize();
        };
        window.addEventListener("resize", this._onWindowResize);
        // Grow/shrink canvas with the workspace wrap when the dialog or browser resizes.
        this._wrapResizeObserver = null;
        if (typeof ResizeObserver === "function" && this.canvas.parentElement) {
            this._wrapResizeObserver = new ResizeObserver(function () {
                self.resize();
            });
            this._wrapResizeObserver.observe(this.canvas.parentElement);
        }
    };

    SceneDesigner.prototype.dispose = function () {
        if (this._wrapResizeObserver) {
            this._wrapResizeObserver.disconnect();
            this._wrapResizeObserver = null;
        }
        if (this._onWindowResize) {
            window.removeEventListener("resize", this._onWindowResize);
            this._onWindowResize = null;
        }
        if (this.canvas) {
            if (this._onCanvasMouseDown) {
                this.canvas.removeEventListener("mousedown", this._onCanvasMouseDown);
            }
            if (this._onCanvasMouseMove) {
                this.canvas.removeEventListener("mousemove", this._onCanvasMouseMove);
            }
            if (this._onCanvasDblClick) {
                this.canvas.removeEventListener("dblclick", this._onCanvasDblClick);
            }
            if (this._onCanvasWheel) {
                this.canvas.removeEventListener("wheel", this._onCanvasWheel);
            }
            if (this.canvas.__sceneDesigner === this) {
                this.canvas.__sceneDesigner = null;
            }
        }
        if (this._onWindowMouseMove) {
            window.removeEventListener("mousemove", this._onWindowMouseMove);
        }
        if (this._onWindowMouseUp) {
            window.removeEventListener("mouseup", this._onWindowMouseUp);
        }
        if (this._onWindowKeyDown) {
            window.removeEventListener("keydown", this._onWindowKeyDown);
        }
        if (this._onWindowKeyUp) {
            window.removeEventListener("keyup", this._onWindowKeyUp);
        }
        this._onCanvasMouseDown = null;
        this._onCanvasMouseMove = null;
        this._onCanvasDblClick = null;
        this._onCanvasWheel = null;
        this._onWindowMouseMove = null;
        this._onWindowMouseUp = null;
        this._onWindowKeyDown = null;
        this._onWindowKeyUp = null;
        this.dragOrbit = false;
        this.dragHandle = null;
        this._keysDown = {};
        this.stopFlyLoop();
        this.onChange = null;
        this.disposeThree();
    };

    SceneDesigner.prototype.hasEye = function () {
        return isFinite(this.eyeX) && isFinite(this.eyeY) && isFinite(this.eyeZ);
    };

    SceneDesigner.prototype.persistEyeToModel = function () {
        if (!this.hasEye()) {
            return;
        }
        this.model.eyeX = this.eyeX;
        this.model.eyeY = this.eyeY;
        this.model.eyeZ = this.eyeZ;
        this.model.yaw = this.yaw;
        this.model.pitch = this.pitch;
        this.model.perspective = true;
    };

    SceneDesigner.prototype.ensureEyeFromOrbit = function () {
        if (this.hasEye()) {
            return;
        }
        var seeded = orbitEyeFromAngles(this.model, this.yaw, this.pitch, this.viewZoom);
        this.eyeX = seeded.x;
        this.eyeY = seeded.y;
        this.eyeZ = seeded.z;
        this.persistEyeToModel();
    };

    SceneDesigner.prototype.startFlyLoop = function () {
        if (this._flyRaf) {
            return;
        }
        var self = this;
        this._flyLastTs = 0;
        function tick(ts) {
            self._flyRaf = 0;
            if (!self.viewMode) {
                self._flyLastTs = 0;
                return;
            }
            var moving = self._keysDown.w || self._keysDown.a || self._keysDown.s || self._keysDown.d;
            if (!moving) {
                self._flyLastTs = 0;
                return;
            }
            var last = self._flyLastTs || ts;
            var dt = Math.min(0.05, Math.max(0, (ts - last) / 1000));
            self._flyLastTs = ts;
            self.ensureEyeFromOrbit();
            var forward = lookForwardFromAngles(self.yaw, self.pitch);
            var flatLen = Math.sqrt(forward.x * forward.x + forward.z * forward.z);
            var fx = flatLen > 1e-6 ? forward.x / flatLen : 0;
            var fz = flatLen > 1e-6 ? forward.z / flatLen : 0;
            var rightX = fz;
            var rightZ = -fx;
            var speed = 0.55 * Math.max(0.35, self.viewZoom || 1);
            var step = speed * dt;
            // W = up, S = down; A/D = ground strafe; mouse wheel = zoom.
            if (self._keysDown.w) {
                self.eyeY += step;
                self._flyMoved = true;
            }
            if (self._keysDown.s) {
                self.eyeY -= step;
                self._flyMoved = true;
            }
            if (self._keysDown.a) {
                self.eyeX -= rightX * step;
                self.eyeZ -= rightZ * step;
                self._flyMoved = true;
            }
            if (self._keysDown.d) {
                self.eyeX += rightX * step;
                self.eyeZ += rightZ * step;
                self._flyMoved = true;
            }
            self.persistEyeToModel();
            if (typeof self.onChange === "function") {
                self.onChange();
            }
            self.draw();
            self._flyRaf = window.requestAnimationFrame(tick);
        }
        this._flyRaf = window.requestAnimationFrame(tick);
    };

    SceneDesigner.prototype.stopFlyLoop = function () {
        if (this._flyRaf) {
            window.cancelAnimationFrame(this._flyRaf);
            this._flyRaf = 0;
        }
        this._flyLastTs = 0;
    };

    SceneDesigner.prototype.disposeThree = function () {
        var three = this._three;
        if (!three) {
            return;
        }
        if (three.root) {
            three.scene.remove(three.root);
            three.root.traverse(function (obj) {
                if (obj.geometry) {
                    obj.geometry.dispose();
                }
                if (obj.material) {
                    if (Array.isArray(obj.material)) {
                        obj.material.forEach(function (mat) { mat.dispose(); });
                    } else {
                        obj.material.dispose();
                    }
                }
            });
            three.root = null;
        }
        if (three.renderer) {
            three.renderer.dispose();
        }
        if (three.glCanvas && three.glCanvas.parentNode) {
            three.glCanvas.parentNode.removeChild(three.glCanvas);
        }
        this._three = null;
    };

    SceneDesigner.prototype.ensureThree = function () {
        var THREE = global.THREE;
        if (!THREE) {
            return null;
        }
        if (this._three) {
            return this._three;
        }
        if (this._threeUnavailable) {
            return null;
        }
        var wrap = this.canvas.parentElement;
        if (!wrap) {
            return null;
        }
        var attempts = [
            {
                antialias: true,
                alpha: false,
                preserveDrawingBuffer: true,
                powerPreference: "default",
                failIfMajorPerformanceCaveat: false
            },
            {
                antialias: false,
                alpha: false,
                preserveDrawingBuffer: true,
                powerPreference: "low-power",
                failIfMajorPerformanceCaveat: false
            }
        ];
        var renderer = null;
        var glCanvas = null;
        var attemptIndex;
        for (attemptIndex = 0; attemptIndex < attempts.length; attemptIndex++) {
            glCanvas = document.createElement("canvas");
            glCanvas.className = "scene-designer-gl-canvas";
            wrap.insertBefore(glCanvas, this.canvas);
            try {
                renderer = new THREE.WebGLRenderer({
                    canvas: glCanvas,
                    antialias: attempts[attemptIndex].antialias,
                    alpha: attempts[attemptIndex].alpha,
                    preserveDrawingBuffer: attempts[attemptIndex].preserveDrawingBuffer,
                    powerPreference: attempts[attemptIndex].powerPreference,
                    failIfMajorPerformanceCaveat: attempts[attemptIndex].failIfMajorPerformanceCaveat
                });
                break;
            } catch (error) {
                renderer = null;
                if (glCanvas.parentNode) {
                    glCanvas.parentNode.removeChild(glCanvas);
                }
                glCanvas = null;
            }
        }
        if (!renderer || !glCanvas) {
            // Stop retry storms on pointer-move (leaked contexts disable GPU).
            this._threeUnavailable = true;
            return null;
        }
        renderer.setClearColor(0x10131a, 1);
        renderer.setPixelRatio(Math.min(2, global.devicePixelRatio || 1));
        var scene = new THREE.Scene();
        // Bright ambient so walls/floors read clearly; soft key light for mild form.
        var ambient = new THREE.AmbientLight(0xffffff, 0.95);
        scene.add(ambient);
        var key = new THREE.DirectionalLight(0xffffff, 0.28);
        key.position.set(0.45, 1.1, 0.35);
        scene.add(key);
        this._three = {
            THREE: THREE,
            renderer: renderer,
            scene: scene,
            ambient: ambient,
            key: key,
            glCanvas: glCanvas,
            root: null,
            camera: null,
            perspCamera: null,
            orthoCamera: null
        };
        return this._three;
    };

    SceneDesigner.prototype.rebuildThreeRoot = function () {
        var three = this.ensureThree();
        if (!three) {
            return;
        }
        if (three.root) {
            three.scene.remove(three.root);
            three.root.traverse(function (obj) {
                if (obj.geometry) {
                    obj.geometry.dispose();
                }
                if (obj.material) {
                    if (Array.isArray(obj.material)) {
                        obj.material.forEach(function (mat) { mat.dispose(); });
                    } else {
                        obj.material.dispose();
                    }
                }
            });
        }
        three.root = buildThreeSceneRoot(this.model, three.THREE);
        three.scene.add(three.root);
    };

    SceneDesigner.prototype.syncThreeCamera = function (width, height) {
        var three = this.ensureThree();
        if (!three) {
            return;
        }
        var THREE = three.THREE;
        var aspect = width / Math.max(1, height);
        if (!three.perspCamera) {
            three.perspCamera = new THREE.PerspectiveCamera(42, aspect, 0.05, 500);
        }
        three.perspCamera.aspect = aspect;
        three.perspCamera.updateProjectionMatrix();
        three.camera = three.perspCamera;
        this.perspective = true;
        this.model.perspective = true;
        var orbit = orbitEyeFromAngles(this.model, this.yaw, this.pitch, this.viewZoom);
        if (!this._flyMoved) {
            this.eyeX = orbit.x;
            this.eyeY = orbit.y;
            this.eyeZ = orbit.z;
            this.persistEyeToModel();
            three.camera.position.set(orbit.x, orbit.y, orbit.z);
            three.camera.lookAt(orbit.targetX, orbit.targetY, orbit.targetZ);
        } else {
            this.ensureEyeFromOrbit();
            var forward = lookForwardFromAngles(this.yaw, this.pitch);
            three.camera.position.set(this.eyeX, this.eyeY, this.eyeZ);
            three.camera.lookAt(
                this.eyeX + forward.x,
                this.eyeY + forward.y,
                this.eyeZ + forward.z
            );
        }
        three.camera.updateMatrixWorld();
    };

    SceneDesigner.prototype.renderThree = function (width, height) {
        var three = this.ensureThree();
        if (!three) {
            return false;
        }
        if (!three.root) {
            this.rebuildThreeRoot();
        }
        three.renderer.setSize(width, height, false);
        this.syncThreeCamera(width, height);
        three.renderer.render(three.scene, three.camera);
        return true;
    };

    SceneDesigner.prototype.resize = function () {
        var parent = this.canvas.parentElement;
        // Exact layout size of the workspace — never invent a larger bitmap than the box
        // (that made CSS width/height:100% stretch/squash the drawing).
        var width = parent && parent.clientWidth > 0 ? parent.clientWidth : 640;
        var height = parent && parent.clientHeight > 0 ? parent.clientHeight : 420;
        if (this.canvas.width === width && this.canvas.height === height
            && this.canvas.style.width === width + "px"
            && this.canvas.style.height === height + "px") {
            return;
        }
        this.canvas.width = width;
        this.canvas.height = height;
        this.canvas.style.width = width + "px";
        this.canvas.style.height = height + "px";
        if (this._three && this._three.glCanvas) {
            this._three.glCanvas.width = width;
            this._three.glCanvas.height = height;
            this._three.glCanvas.style.width = width + "px";
            this._three.glCanvas.style.height = height + "px";
        }
        this.draw();
    };

    SceneDesigner.prototype.setModel = function (model) {
        this.model = model && model.floors ? cloneJson(model) : defaultModel();
        if (!(this.model.wallThickness > 0)) {
            this.model.wallThickness = DEFAULT_WALL_THICKNESS;
        }
        if (!(this.model.wallHeight > 0)) {
            this.model.wallHeight = DEFAULT_WALL_HEIGHT;
        }
        normalizeSceneModel(this.model);
        var zoom = Number(this.model.viewZoom);
        this.viewZoom = (isFinite(zoom) && zoom > 0)
            ? Math.max(0.35, Math.min(3.5, zoom))
            : 1;
        this.model.viewZoom = this.viewZoom;
        var yaw = Number(this.model.yaw);
        this.yaw = isFinite(yaw) ? yaw : 0.85;
        this.model.yaw = this.yaw;
        var pitch = Number(this.model.pitch);
        this.pitch = isFinite(pitch)
            ? Math.max(0.28, Math.min(1.25, pitch))
            : 0.62;
        this.model.pitch = this.pitch;
        this.perspective = true;
        this.model.perspective = true;
        var eyeX = Number(this.model.eyeX);
        var eyeY = Number(this.model.eyeY);
        var eyeZ = Number(this.model.eyeZ);
        if (isFinite(eyeX) && isFinite(eyeY) && isFinite(eyeZ)) {
            this.eyeX = eyeX;
            this.eyeY = eyeY;
            this.eyeZ = eyeZ;
            this._flyMoved = true;
            this.persistEyeToModel();
        } else {
            this.eyeX = null;
            this.eyeY = null;
            this.eyeZ = null;
            this._flyMoved = false;
            this.model.eyeX = null;
            this.model.eyeY = null;
            this.model.eyeZ = null;
        }
        this.editZoom = 1;
        this.floorIndex = 0;
        this.selection = null;
        this.dragHandle = null;
        this.dragMoved = false;
        this.dragCornerFrom = null;
        this.dragCornerOrigin = null;
        this.dragShiftHeld = false;
        this.draw();
    };

    SceneDesigner.prototype.getModel = function () {
        this.model.viewZoom = this.viewZoom;
        this.model.yaw = this.yaw;
        this.model.pitch = this.pitch;
        this.model.perspective = true;
        // Always materialize a concrete eye so bake + JSON share the same camera pose.
        this.ensureEyeFromOrbit();
        this.persistEyeToModel();
        return cloneJson(this.model);
    };

    SceneDesigner.prototype.setPerspective = function () {
        // Orthographic / isometric projection removed — View scene is perspective only.
        this.perspective = true;
        this.model.perspective = true;
        this.draw();
        return this.perspective;
    };

    SceneDesigner.prototype.togglePerspective = function () {
        return this.setPerspective(true);
    };

    SceneDesigner.prototype.setTool = function (tool) {
        this.tool = tool || "wall";
        this.draw();
        return false;
    };

    SceneDesigner.prototype.applyTool = function (tool) {
        this.tool = tool || "wall";
        if (tool === "wall") {
            this.addWall();
            return true;
        }
        if ((tool === "window" || tool === "door") && this.placeOpeningOnSelectedWall(tool)) {
            this.draw();
            return true;
        }
        this.draw();
        return false;
    };

    SceneDesigner.prototype.addWall = function () {
        var size = this.model.sceneSize || 1;
        var length = size / 7;
        if (!Array.isArray(this.model.walls)) {
            this.model.walls = [];
        }
        var offset = (this.model.walls.length % 6) * (length * 0.2);
        var midZ = offset - length * 0.4;
        this.model.walls.push({
            x1: -length * 0.5 + offset * 0.25,
            z1: midZ,
            x2: length * 0.5 + offset * 0.25,
            z2: midZ
        });
        this.selection = { list: "walls", index: this.model.walls.length - 1 };
        this.notify();
        this.draw();
    };

    SceneDesigner.prototype.setViewMode = function (enabled) {
        this.viewMode = !!enabled;
        this.dragHandle = null;
        this._keysDown = {};
        this.stopFlyLoop();
        var wrap = this.canvas && this.canvas.parentElement;
        if (wrap) {
            wrap.classList.toggle("is-view-mode", this.viewMode);
        }
        if (!this.viewMode) {
            // Free GPU while editing the plan.
            this.disposeThree();
            this.draw();
            return;
        }
        // Entering View always frames the scene center (mouse drag orbits that center).
        this._flyMoved = false;
        this.eyeX = null;
        this.eyeY = null;
        this.eyeZ = null;
        this.ensureEyeFromOrbit();
        var self = this;
        loadThreeJs().then(function () {
            if (!self.viewMode) {
                return;
            }
            self.draw();
        }).catch(function () {
            self.draw();
        });
    };

    SceneDesigner.prototype.toggleViewMode = function () {
        this.setViewMode(!this.viewMode);
        return this.viewMode;
    };

    SceneDesigner.prototype.addFloor = function () {
        var last = this.model.floors[this.model.floors.length - 1];
        var floor = {
            y: 0,
            height: 0.3,
            color: (last && last.color) ? last.color : DEFAULT_FLOOR_COLOR,
            polygon: defaultPolygon(this.model.sceneSize || 1)
        };
        this.model.floors.push(floor);
        this.floorIndex = this.model.floors.length - 1;
        this.selection = { list: "floor", floorIndex: this.floorIndex };
        this.notify();
        this.draw();
    };

    SceneDesigner.prototype.getCurrentFloorColor = function () {
        var floor = this.currentFloor();
        return floor && floor.color ? floor.color : DEFAULT_FLOOR_COLOR;
    };

    SceneDesigner.prototype.setCurrentFloorColor = function (color) {
        var floor = this.currentFloor();
        if (!floor) {
            return;
        }
        var next = String(color || "").trim();
        if (!/^#[0-9a-fA-F]{6}$/.test(next)) {
            return;
        }
        floor.color = next.toLowerCase();
        this.notify();
        this.draw();
    };

    SceneDesigner.prototype.getWallThickness = function () {
        return this.model.wallThickness > 0 ? this.model.wallThickness : DEFAULT_WALL_THICKNESS;
    };

    SceneDesigner.prototype.setWallThickness = function (value) {
        var next = Number(value);
        if (!isFinite(next)) {
            return;
        }
        if (next < 0.005) {
            next = 0.005;
        } else if (next > 0.25) {
            next = 0.25;
        }
        this.model.wallThickness = next;
        this.notify();
        this.draw();
    };

    SceneDesigner.prototype.getWallHeight = function () {
        return this.model.wallHeight > 0 ? this.model.wallHeight : DEFAULT_WALL_HEIGHT;
    };

    SceneDesigner.prototype.setWallHeight = function (value) {
        var next = Number(value);
        if (!isFinite(next)) {
            return;
        }
        if (next < 0.05) {
            next = 0.05;
        } else if (next > 1.5) {
            next = 1.5;
        }
        this.model.wallHeight = next;
        // View mode keeps a cached Three.js mesh — rebuild so height updates immediately.
        if (this.viewMode && this._three) {
            this.rebuildThreeRoot();
        }
        this.notify();
        this.draw();
    };

    SceneDesigner.prototype.placeOpeningOnSelectedWall = function (kind) {
        if (!this.selection || this.selection.list !== "walls") {
            return false;
        }
        var wall = (this.model.walls || [])[this.selection.index];
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
        var t0 = ((midX - ux * half) - wall.x1) * ux + ((midZ - uz * half) - wall.z1) * uz;
        t0 /= len;
        var t1 = ((midX + ux * half) - wall.x1) * ux + ((midZ + uz * half) - wall.z1) * uz;
        t1 /= len;
        if (t0 > t1) {
            var swap = t0;
            t0 = t1;
            t1 = swap;
        }
        var seg = {
            wallIndex: this.selection.index,
            t0: t0,
            t1: t1,
            x1: midX - ux * half,
            z1: midZ - uz * half,
            x2: midX + ux * half,
            z2: midZ + uz * half
        };
        var listName = kind === "door" ? "doors" : "windows";
        if (!Array.isArray(this.model[listName])) {
            this.model[listName] = [];
        }
        this.model[listName].push(seg);
        this.selection = { list: listName, index: this.model[listName].length - 1 };
        this.notify();
        return true;
    };

    SceneDesigner.prototype.removeCurrentFloor = function () {
        // Remove floor plate only — walls/windows/doors stay on the scene model.
        if (this.model.floors.length > 1) {
            this.model.floors.splice(this.floorIndex, 1);
            if (this.floorIndex >= this.model.floors.length) {
                this.floorIndex = this.model.floors.length - 1;
            }
        } else {
            var color = this.getCurrentFloorColor();
            this.model.floors[0] = {
                y: 0,
                height: 0.3,
                color: color,
                polygon: defaultPolygon(this.model.sceneSize || 1)
            };
            this.floorIndex = 0;
        }
        this.selection = null;
        this.notify();
        this.draw();
        return true;
    };

    SceneDesigner.prototype.removeSelected = function () {
        if (!this.selection) {
            return false;
        }
        if (typeof this.selection.floorIndex === "number"
            && this.selection.floorIndex !== this.floorIndex
            && this.model.floors[this.selection.floorIndex]) {
            this.floorIndex = this.selection.floorIndex;
        }
        if (this.selection.list === "floor" || this.selection.list === "polygon") {
            var floorPolygon = this.currentFloor();
            if (!floorPolygon || !floorPolygon.polygon) {
                return false;
            }
            // Floor plate only — never delete walls/windows/doors with the floor.
            if (this.selection.list === "floor" || floorPolygon.polygon.length <= 3) {
                if (this.model.floors.length > 1) {
                    this.model.floors.splice(this.floorIndex, 1);
                    if (this.floorIndex >= this.model.floors.length) {
                        this.floorIndex = this.model.floors.length - 1;
                    }
                } else {
                    floorPolygon.polygon = [];
                }
                this.selection = null;
                this.notify();
                this.draw();
                return true;
            }
            if (this.selection.index >= 0
                && this.selection.index < floorPolygon.polygon.length) {
                floorPolygon.polygon.splice(this.selection.index, 1);
                this.selection = null;
                this.notify();
                this.draw();
                return true;
            }
            return false;
        }
        var list = this.model[this.selection.list];
        if (list && this.selection.index >= 0 && this.selection.index < list.length) {
            list.splice(this.selection.index, 1);
            syncModelOpenings(this.model);
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
        return this.worldFromClient(event.clientX, event.clientY);
    };

    SceneDesigner.prototype.planScale = function () {
        // Fixed px/world — canvas resize changes visible workspace, not object size.
        return planScaleFor(this.model.sceneSize || 1, this.editZoom || 1);
    };

    SceneDesigner.prototype.worldFromClient = function (clientX, clientY) {
        var rect = this.canvas.getBoundingClientRect();
        if (rect.width <= 0 || rect.height <= 0) {
            return { x: 0, z: 0 };
        }
        var planScale = this.planScale();
        if (planScale <= 0) {
            return { x: 0, z: 0 };
        }
        return {
            x: (clientX - rect.left - rect.width * 0.5) / planScale,
            z: (clientY - rect.top - rect.height * 0.5) / planScale
        };
    };

    SceneDesigner.prototype.dragThresholdPx = function () {
        return 5;
    };

    SceneDesigner.prototype.handleHitSize = function () {
        return (this.model.sceneSize || 1) * 0.028;
    };

    SceneDesigner.prototype.floorForHandle = function (handle) {
        if (handle && typeof handle.floorIndex === "number"
            && this.model.floors[handle.floorIndex]) {
            return this.model.floors[handle.floorIndex];
        }
        return this.currentFloor();
    };

    SceneDesigner.prototype.findFloorIndexAt = function (point) {
        var floors = this.model.floors || [];
        var index;
        // Top-most drawn floor wins (later index painted later).
        for (index = floors.length - 1; index >= 0; index--) {
            if (pointInPolygon(point, floors[index].polygon || [])) {
                return index;
            }
        }
        return -1;
    };

    SceneDesigner.prototype.findHandleAt = function (point) {
        var threshold = this.handleHitSize();
        var best = null;
        var bestDist = threshold;
        var self = this;
        var tieEps = threshold * 0.15;

        function consider(candidate, dist, preferWall) {
            if (dist > bestDist + tieEps) {
                return;
            }
            if (dist < bestDist - tieEps) {
                bestDist = dist;
                best = candidate;
                return;
            }
            // Near-tie: wall/window/door endpoints win over floor corners in the same hit zone.
            if (preferWall && best && best.list === "polygon") {
                bestDist = dist;
                best = candidate;
                return;
            }
            if (!preferWall && best && best.list !== "polygon") {
                return;
            }
            if (dist < bestDist) {
                bestDist = dist;
                best = candidate;
            }
        }

        // Floor polygon corners for EVERY floor so inactive plates stay editable.
        (this.model.floors || []).forEach(function (floor, floorIndex) {
            (floor.polygon || []).forEach(function (corner, index) {
                var dx = corner.x - point.x;
                var dz = corner.z - point.z;
                consider({
                    list: "polygon",
                    index: index,
                    end: 0,
                    floorIndex: floorIndex
                }, Math.sqrt(dx * dx + dz * dz), false);
            });
        });
        // All scene walls/openings — never gated by the selected floor plate.
        ["walls", "windows", "doors"].forEach(function (listName) {
            var list = self.model[listName] || [];
            list.forEach(function (seg, index) {
                [
                    { end: 1, x: seg.x1, z: seg.z1 },
                    { end: 2, x: seg.x2, z: seg.z2 }
                ].forEach(function (handle) {
                    var dx = handle.x - point.x;
                    var dz = handle.z - point.z;
                    consider({
                        list: listName,
                        index: index,
                        end: handle.end
                    }, Math.sqrt(dx * dx + dz * dz), true);
                });
            });
        });
        return best;
    };

    SceneDesigner.prototype.snapJoin = function (point, ignoreHandle) {
        var threshold = (this.model.sceneSize || 1) * 0.03;
        var best = null;
        var bestDist = threshold;
        var self = this;
        ["walls", "windows", "doors"].forEach(function (listName) {
            var list = self.model[listName] || [];
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
        var threshold = (this.model.sceneSize || 1) * 0.025;
        var best = null;
        var bestDist = threshold;
        var self = this;

        // Walls/openings first — independent of which floor plate is focused.
        ["walls", "windows", "doors"].forEach(function (listName) {
            (self.model[listName] || []).forEach(function (seg, index) {
                var dist = distPointToSeg(point.x, point.z, seg);
                if (dist < bestDist) {
                    bestDist = dist;
                    best = { list: listName, index: index };
                }
            });
        });
        if (best) {
            this.selection = best;
            this.draw();
            return best;
        }

        // Then floor polygon edges (not fill — fill is handled in onPointerDown).
        (this.model.floors || []).forEach(function (floor, floorIndex) {
            var polygon = floor.polygon || [];
            var index;
            for (index = 0; index < polygon.length; index++) {
                var a = polygon[index];
                var b = polygon[(index + 1) % polygon.length];
                var dist = distPointToSeg(point.x, point.z, {
                    x1: a.x, z1: a.z, x2: b.x, z2: b.z
                });
                if (dist < bestDist) {
                    bestDist = dist;
                    best = { list: "polygon", index: index, floorIndex: floorIndex };
                }
            }
        });
        if (best && typeof best.floorIndex === "number") {
            this.floorIndex = best.floorIndex;
        }
        this.selection = best;
        this.draw();
        return best;
    };

    SceneDesigner.prototype.insertVertexOnWall = function (point) {
        var threshold = (this.model.sceneSize || 1) * 0.03;
        var bestIndex = -1;
        var bestDist = threshold;
        var bestProj = null;
        (this.model.walls || []).forEach(function (seg, index) {
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
        var wall = this.model.walls[bestIndex];
        var second = { x1: bestProj.x, z1: bestProj.z, x2: wall.x2, z2: wall.z2 };
        wall.x2 = bestProj.x;
        wall.z2 = bestProj.z;
        this.model.walls.splice(bestIndex + 1, 0, second);
        // Wall indices after the split shift — rebind openings by proximity.
        var self = this;
        ["windows", "doors"].forEach(function (listName) {
            (self.model[listName] || []).forEach(function (opening) {
                delete opening.wallIndex;
                delete opening.t0;
                delete opening.t1;
            });
        });
        syncModelOpenings(this.model);
        this.selection = { list: "walls", index: bestIndex + 1 };
        this.notify();
        this.draw();
        return true;
    };

    SceneDesigner.prototype.insertVertexOnFloorEdge = function (point) {
        var floor = this.currentFloor();
        var polygon = floor.polygon || [];
        if (polygon.length < 3) {
            return false;
        }
        var threshold = (this.model.sceneSize || 1) * 0.03;
        var bestIndex = -1;
        var bestDist = threshold;
        var bestProj = null;
        var index;
        for (index = 0; index < polygon.length; index++) {
            var a = polygon[index];
            var b = polygon[(index + 1) % polygon.length];
            var seg = { x1: a.x, z1: a.z, x2: b.x, z2: b.z };
            var dist = distPointToSeg(point.x, point.z, seg);
            if (dist < bestDist) {
                bestDist = dist;
                bestIndex = index;
                bestProj = projectOnSeg(point.x, point.z, seg);
            }
        }
        if (bestIndex < 0 || !bestProj || bestProj.t <= 0.05 || bestProj.t >= 0.95) {
            return false;
        }
        polygon.splice(bestIndex + 1, 0, { x: bestProj.x, z: bestProj.z });
        this.selection = { list: "polygon", index: bestIndex + 1 };
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
        if (this.insertVertexOnWall(point)) {
            return;
        }
        this.insertVertexOnFloorEdge(point);
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
            if (typeof handle.floorIndex === "number" && handle.floorIndex !== this.floorIndex) {
                this.floorIndex = handle.floorIndex;
            }
            // Exclusive single-handle selection; move only after drag threshold.
            this.dragHandle = handle;
            this.dragMoved = false;
            this.dragStartClientX = event.clientX;
            this.dragStartClientY = event.clientY;
            this.selection = {
                list: handle.list,
                index: handle.index,
                floorIndex: handle.floorIndex
            };
            var corner;
            if (handle.list === "polygon") {
                var polyFloor = this.floorForHandle(handle);
                corner = polyFloor.polygon[handle.index];
            } else {
                var handleSeg = (this.model[handle.list] || [])[handle.index];
                corner = handle.end === 1
                    ? { x: handleSeg.x1, z: handleSeg.z1 }
                    : { x: handleSeg.x2, z: handleSeg.z2 };
            }
            this.dragCornerFrom = { x: corner.x, z: corner.z };
            this.dragCornerOrigin = { x: corner.x, z: corner.z };
            this.dragShiftHeld = !!event.shiftKey;
            this.lastDragClientX = event.clientX;
            this.lastDragClientY = event.clientY;
            this.draw();
            return;
        }
        // Prefer wall/opening segments over floor fill so floors never "own" walls.
        if (this.selectNear(point)) {
            return;
        }
        // Floor plate click selects the floor only — does not affect wall editability.
        var floorHit = this.findFloorIndexAt(point);
        if (floorHit >= 0) {
            this.floorIndex = floorHit;
            this.selection = { list: "floor", floorIndex: floorHit };
            this.draw();
        }
    };

    SceneDesigner.prototype.applyHandleDragAtClient = function (clientX, clientY, shiftHeld) {
        if (!this.dragHandle || this.viewMode || !this.dragMoved) {
            return;
        }
        var floor = this.floorForHandle(this.dragHandle);
        if (!this.dragCornerFrom) {
            return;
        }
        var raw = this.worldFromClient(clientX, clientY);
        var locked = resolveShiftDragPoint(
            this.model,
            floor,
            this.dragHandle,
            this.dragCornerOrigin,
            raw,
            shiftHeld,
            this.model.sceneSize || 1
        );
        var point = smoothToward(this.dragCornerFrom, locked, shiftHeld ? 1 : 0.65);
        if (this.dragHandle.list === "polygon") {
            var corner = floor.polygon && floor.polygon[this.dragHandle.index];
            if (!corner) {
                return;
            }
            corner.x = point.x;
            corner.z = point.z;
            this.dragCornerFrom = { x: point.x, z: point.z };
            this.draw();
            return;
        }
        var list = this.model[this.dragHandle.list];
        if (!list || !list[this.dragHandle.index]) {
            return;
        }
        if (this.dragHandle.list === "walls") {
            // Move only this wall's shared wall-corner group — never the floor polygon.
            moveSharedWallCorner(
                this.model.walls,
                this.dragCornerFrom.x,
                this.dragCornerFrom.z,
                point.x,
                point.z
            );
            syncModelOpenings(this.model);
            this.dragCornerFrom = { x: point.x, z: point.z };
            this.draw();
            return;
        }
        var seg = list[this.dragHandle.index];
        if (this.dragHandle.end === 1) {
            seg.x1 = point.x;
            seg.z1 = point.z;
        } else {
            seg.x2 = point.x;
            seg.z2 = point.z;
        }
        if (this.dragHandle.list === "windows" || this.dragHandle.list === "doors") {
            var walls = this.model.walls || [];
            var hostWall = (typeof seg.wallIndex === "number")
                ? walls[seg.wallIndex]
                : null;
            if (hostWall) {
                var p1 = projectOnSeg(seg.x1, seg.z1, hostWall);
                var p2 = projectOnSeg(seg.x2, seg.z2, hostWall);
                seg.t0 = Math.min(p1.t, p2.t);
                seg.t1 = Math.max(p1.t, p2.t);
                syncOpeningGeometry(seg, walls);
            } else {
                bindOpeningToNearestWall(seg, walls);
                syncOpeningGeometry(seg, walls);
            }
        }
        this.dragCornerFrom = { x: point.x, z: point.z };
        this.draw();
    };

    SceneDesigner.prototype.finishHandleDrag = function (event) {
        if (!this.dragHandle) {
            return;
        }
        // Click-select without crossing drag threshold: keep selection, do not relocate.
        if (!this.dragMoved) {
            this.dragHandle = null;
            this.dragCornerFrom = null;
            this.dragCornerOrigin = null;
            this.dragShiftHeld = false;
            this.dragMoved = false;
            this.draw();
            return;
        }
        var floor = this.floorForHandle(this.dragHandle);
        if (this.dragCornerFrom) {
            var shiftHeld = event ? !!event.shiftKey : this.dragShiftHeld;
            var raw = event
                ? this.worldFromClient(event.clientX, event.clientY)
                : { x: this.dragCornerFrom.x, z: this.dragCornerFrom.z };
            var locked = resolveShiftDragPoint(
                this.model,
                floor,
                this.dragHandle,
                this.dragCornerOrigin,
                raw,
                shiftHeld,
                this.model.sceneSize || 1
            );
            if (this.dragHandle.list === "polygon") {
                var corner = floor.polygon && floor.polygon[this.dragHandle.index];
                if (corner) {
                    corner.x = locked.x;
                    corner.z = locked.z;
                }
            } else if (shiftHeld && this.dragHandle.list === "walls") {
                moveSharedWallCorner(
                    this.model.walls,
                    this.dragCornerFrom.x,
                    this.dragCornerFrom.z,
                    locked.x,
                    locked.z
                );
                syncModelOpenings(this.model);
            } else {
                var snapped = this.snapJoin(locked, this.dragHandle);
                if (this.dragHandle.list === "walls") {
                    moveSharedWallCorner(
                        this.model.walls,
                        this.dragCornerFrom.x,
                        this.dragCornerFrom.z,
                        snapped.x,
                        snapped.z
                    );
                    syncModelOpenings(this.model);
                } else {
                    var list = this.model[this.dragHandle.list];
                    var seg = list && list[this.dragHandle.index];
                    if (seg) {
                        if (this.dragHandle.end === 1) {
                            seg.x1 = snapped.x;
                            seg.z1 = snapped.z;
                        } else {
                            seg.x2 = snapped.x;
                            seg.z2 = snapped.z;
                        }
                        if (this.dragHandle.list === "windows" || this.dragHandle.list === "doors") {
                            bindOpeningToNearestWall(seg, this.model.walls || []);
                            syncOpeningGeometry(seg, this.model.walls || []);
                        }
                    }
                }
            }
        }
        this.dragHandle = null;
        this.dragCornerFrom = null;
        this.dragCornerOrigin = null;
        this.dragShiftHeld = false;
        this.dragMoved = false;
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
            this.pitch = Math.max(0.28, Math.min(1.25, this.pitch + dy * 0.01));
            this.model.yaw = this.yaw;
            this.model.pitch = this.pitch;
            // Always orbit the scene center so drag feels like the room rotating.
            this._flyMoved = false;
            this.eyeX = null;
            this.eyeY = null;
            this.eyeZ = null;
            this.ensureEyeFromOrbit();
            if (typeof this.onChange === "function") {
                this.onChange();
            }
            this.draw();
            return;
        }
        if (!this.dragHandle || this.viewMode) {
            return;
        }
        this.lastDragClientX = event.clientX;
        this.lastDragClientY = event.clientY;
        this.dragShiftHeld = !!event.shiftKey;
        if (!this.dragMoved) {
            var moveDx = event.clientX - this.dragStartClientX;
            var moveDy = event.clientY - this.dragStartClientY;
            if ((moveDx * moveDx + moveDy * moveDy) < this.dragThresholdPx() * this.dragThresholdPx()) {
                return;
            }
            this.dragMoved = true;
        }
        this.applyHandleDragAtClient(event.clientX, event.clientY, this.dragShiftHeld);
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
        var planScale = this.planScale();
        var thicknessPx = Math.max(4, this.getWallThickness() * planScale);
        ctx.fillStyle = "#1b1f2a";
        ctx.fillRect(0, 0, width, height);
        // Square world-space grid (finer): step = sceneSize / 20.
        var gridStep = size / 20;
        var halfWorldX = (width * 0.5) / planScale;
        var halfWorldZ = (height * 0.5) / planScale;
        var gridMinX = Math.floor(-halfWorldX / gridStep) * gridStep;
        var gridMaxX = Math.ceil(halfWorldX / gridStep) * gridStep;
        var gridMinZ = Math.floor(-halfWorldZ / gridStep) * gridStep;
        var gridMaxZ = Math.ceil(halfWorldZ / gridStep) * gridStep;
        var gridCoord;
        var gridIndex;
        for (gridCoord = gridMinX; gridCoord <= gridMaxX + gridStep * 0.5; gridCoord += gridStep) {
            gridIndex = Math.round(gridCoord / gridStep);
            var majorX = gridIndex % 5 === 0;
            ctx.strokeStyle = majorX ? "#3d4a63" : "#2e3648";
            ctx.lineWidth = majorX ? 1.75 : 1;
            var gx = width * 0.5 + gridCoord * planScale;
            ctx.beginPath();
            ctx.moveTo(gx, 0);
            ctx.lineTo(gx, height);
            ctx.stroke();
        }
        for (gridCoord = gridMinZ; gridCoord <= gridMaxZ + gridStep * 0.5; gridCoord += gridStep) {
            gridIndex = Math.round(gridCoord / gridStep);
            var majorZ = gridIndex % 5 === 0;
            ctx.strokeStyle = majorZ ? "#3d4a63" : "#2e3648";
            ctx.lineWidth = majorZ ? 1.75 : 1;
            var gy = height * 0.5 + gridCoord * planScale;
            ctx.beginPath();
            ctx.moveTo(0, gy);
            ctx.lineTo(width, gy);
            ctx.stroke();
        }
        var self = this;
        function toPx(point) {
            return {
                x: width * 0.5 + point.x * planScale,
                y: height * 0.5 + point.z * planScale
            };
        }
        function drawFloorPlate(floor, floorIndex, active) {
            var polygon = floor.polygon || [];
            if (polygon.length < 3) {
                return;
            }
            var floorSelected = self.selection
                && self.selection.list === "floor"
                && (self.selection.floorIndex == null
                    ? floorIndex === self.floorIndex
                    : self.selection.floorIndex === floorIndex);
            ctx.beginPath();
            polygon.forEach(function (corner, index) {
                var p = toPx(corner);
                if (index === 0) {
                    ctx.moveTo(p.x, p.y);
                } else {
                    ctx.lineTo(p.x, p.y);
                }
            });
            ctx.closePath();
            ctx.fillStyle = hexToRgba(
                floor.color || DEFAULT_FLOOR_COLOR,
                floorSelected ? 0.52 : (active ? 0.38 : 0.2)
            );
            ctx.fill();
            ctx.strokeStyle = floorSelected ? "#ce93d8" : (active ? "#9fb6c9" : "#5a6a7c");
            ctx.lineWidth = floorSelected ? 2.5 : 1.5;
            ctx.stroke();
        }
        this.model.floors.forEach(function (floor, index) {
            drawFloorPlate(floor, index, index === self.floorIndex);
        });
        function drawSegs(list, listName, color, widthPx) {
            (list || []).forEach(function (seg, index) {
                // Highlight only the selected wall/opening — never the focused floor plate.
                var selected = self.selection
                    && self.selection.list === listName
                    && self.selection.index === index;
                var a = toPx({ x: seg.x1, z: seg.z1 });
                var b = toPx({ x: seg.x2, z: seg.z2 });
                ctx.strokeStyle = selected ? "#ce93d8" : color;
                ctx.lineWidth = selected ? widthPx + 2 : widthPx;
                ctx.globalAlpha = 1;
                ctx.lineCap = "butt";
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
        function drawPolygonHandles(polygon, floorIndex, active) {
            (polygon || []).forEach(function (corner, index) {
                var selected = self.selection
                    && self.selection.list === "polygon"
                    && self.selection.index === index
                    && (self.selection.floorIndex == null
                        ? floorIndex === self.floorIndex
                        : self.selection.floorIndex === floorIndex);
                var p = toPx(corner);
                ctx.beginPath();
                ctx.arc(p.x, p.y, selected ? 6 : (active ? 5 : 4), 0, Math.PI * 2);
                ctx.fillStyle = selected ? "#ce93d8" : (active ? "#81c784" : "#4e7a52");
                ctx.globalAlpha = active ? 1 : 0.85;
                ctx.fill();
                ctx.globalAlpha = 1;
                ctx.strokeStyle = "#111";
                ctx.lineWidth = 1;
                ctx.stroke();
            });
        }
        syncModelOpenings(this.model);
        // Scene-level walls — always fully drawn/selectable, independent of floor focus.
        drawSegs(this.model.walls, "walls", "#d7dee8", thicknessPx);
        drawSegs(this.model.windows, "windows", "#4fc3f7", Math.max(2, thicknessPx * 0.55));
        drawSegs(this.model.doors, "doors", "#ffb74d", Math.max(2, thicknessPx * 0.55));
        this.model.floors.forEach(function (floor, index) {
            drawPolygonHandles(floor.polygon, index, index === self.floorIndex);
        });
        drawHandles(this.model.walls, "walls", "#eceff1");
        drawHandles(this.model.windows, "windows", "#4fc3f7");
        drawHandles(this.model.doors, "doors", "#ffb74d");
        ctx.fillStyle = "#9aa4b5";
        ctx.font = "12px sans-serif";
        ctx.fillText(
            "Floor " + (this.floorIndex + 1) + "/" + this.model.floors.length
                + " · tool: " + this.tool
                + " · thickness: " + this.getWallThickness().toFixed(2)
                + " · wheel zoom",
            10,
            18
        );
    };

    SceneDesigner.prototype.drawView = function () {
        var ctx = this.ctx;
        var width = this.canvas.width;
        var height = this.canvas.height;
        this.model.perspective = true;
        this.ensureEyeFromOrbit();
        if (this.renderThree(width, height)) {
            // Clear the 2D overlay so the Three.js canvas underneath is visible.
            ctx.clearRect(0, 0, width, height);
            ctx.fillStyle = "#9aa4b5";
            ctx.font = "12px sans-serif";
            ctx.fillText(
                "View scene · drag orbit · W up · S down · A/D strafe · wheel zoom",
                10,
                18
            );
            return;
        }
        ctx.fillStyle = "#10131a";
        ctx.fillRect(0, 0, width, height);
        ctx.fillStyle = "#9aa4b5";
        ctx.font = "12px sans-serif";
        ctx.fillText("View scene · three.js unavailable", 10, 18);
    };

    SceneDesigner.prototype.draw = function () {
        if (this.viewMode) {
            this.drawView();
        } else {
            this.drawTop();
        }
    };

    function contentPixelBounds(ctx, width, height) {
        var image = ctx.getImageData(0, 0, width, height);
        var data = image.data;
        var minX = width;
        var maxX = -1;
        var minY = height;
        var maxY = -1;
        var y;
        var x;
        for (y = 0; y < height; y++) {
            var row = y * width * 4;
            for (x = 0; x < width; x++) {
                var i = row + x * 4;
                // Anything brighter/different from near-black background counts as content.
                if (data[i] > 28 || data[i + 1] > 32 || data[i + 2] > 40) {
                    if (x < minX) {
                        minX = x;
                    }
                    if (x > maxX) {
                        maxX = x;
                    }
                    if (y < minY) {
                        minY = y;
                    }
                    if (y > maxY) {
                        maxY = y;
                    }
                }
            }
        }
        if (maxX < minX || maxY < minY) {
            return { minX: 0, maxX: width - 1, minY: 0, maxY: height - 1 };
        }
        return { minX: minX, maxX: maxX, minY: minY, maxY: maxY };
    }

    SceneDesigner.prototype.bakeJpegFromContext = function (ctx, width, height, scale, quality) {
        var pad = Math.max(1, Math.round((96 / 2.54) * scale));
        var bounds = contentPixelBounds(ctx, width, height);
        var cropLeft = Math.max(0, bounds.minX - pad);
        var cropTop = Math.max(0, bounds.minY - pad);
        var cropRight = Math.min(width - 1, bounds.maxX + pad);
        var cropBottom = Math.min(height - 1, bounds.maxY + pad);
        var cropWidth = Math.max(1, cropRight - cropLeft + 1);
        var cropHeight = Math.max(1, cropBottom - cropTop + 1);
        var cropped = document.createElement("canvas");
        cropped.width = cropWidth;
        cropped.height = cropHeight;
        var croppedCtx = cropped.getContext("2d");
        croppedCtx.fillStyle = "#10131a";
        croppedCtx.fillRect(0, 0, cropWidth, cropHeight);
        croppedCtx.drawImage(
            ctx.canvas,
            cropLeft,
            cropTop,
            cropWidth,
            cropHeight,
            0,
            0,
            cropWidth,
            cropHeight
        );
        return new Promise(function (resolve) {
            cropped.toBlob(function (blob) {
                resolve(blob);
            }, "image/jpeg", quality == null ? 0.82 : quality);
        });
    };

    SceneDesigner.prototype.bakeJpegBlob = function (quality) {
        var self = this;
        var srcW = Math.max(1, this.canvas.width || 640);
        var srcH = Math.max(1, this.canvas.height || 420);
        var scale = Math.max(1, Math.min(4, 1920 / Math.max(srcW, srcH)));
        var width = Math.max(1, Math.round(srcW * scale));
        var height = Math.max(1, Math.round(srcH * scale));
        this.model.perspective = true;
        this.ensureEyeFromOrbit();
        this.persistEyeToModel();

        return loadThreeJs().then(function () {
            var wasView = self.viewMode;
            if (!wasView) {
                var wrap = self.canvas && self.canvas.parentElement;
                if (wrap) {
                    wrap.classList.add("is-view-mode");
                }
            }
            if (!self.renderThree(width, height)) {
                if (!wasView) {
                    self.disposeThree();
                    var wrapFail = self.canvas && self.canvas.parentElement;
                    if (wrapFail) {
                        wrapFail.classList.remove("is-view-mode");
                    }
                }
                return Promise.reject(new Error("three.js bake unavailable"));
            }
            var glCanvas = self._three.glCanvas;
            var off = document.createElement("canvas");
            off.width = width;
            off.height = height;
            var ctx = off.getContext("2d");
            ctx.fillStyle = "#10131a";
            ctx.fillRect(0, 0, width, height);
            ctx.drawImage(glCanvas, 0, 0, width, height);
            if (wasView) {
                self.renderThree(self.canvas.width, self.canvas.height);
            } else {
                self.disposeThree();
                var wrapRestore = self.canvas && self.canvas.parentElement;
                if (wrapRestore) {
                    wrapRestore.classList.remove("is-view-mode");
                }
            }
            return self.bakeJpegFromContext(ctx, width, height, scale, quality);
        });
    };

    function scenePinStructureKey(pin) {
        var size = normalizeScenePinSize(pin);
        return [
            clamp01(pin && pin.x),
            clamp01(pin && pin.y),
            size.w.toFixed(4),
            size.h.toFixed(4),
            pin && pin.kind ? pin.kind : "",
            pin && pin.viewStyle ? pin.viewStyle : "",
            pin && pin.ieee ? pin.ieee : "",
            pin && (pin.automation || pin.automationId) ? (pin.automation || pin.automationId) : "",
            pin && pin.param ? pin.param : "",
            pin && pin.channel != null ? pin.channel : "",
            pin && pin.icon ? pin.icon : ""
        ].join("|");
    }

    function defaultScenePinSize(pin) {
        if (pin && pin.viewStyle === "gauge") {
            return { w: 0.20, h: 0.16 };
        }
        return { w: 0.08, h: 0.08 };
    }

    function normalizeScenePinSize(pin) {
        var def = defaultScenePinSize(pin);
        var legacy = parseFloat(pin && (pin.scale != null ? pin.scale : pin.iconScale));
        if (!(legacy > 0) || legacy !== legacy) {
            legacy = 1;
        }
        var w = parseFloat(pin && pin.sizeW);
        var h = parseFloat(pin && pin.sizeH);
        if (!(w > 0) || w !== w) {
            w = def.w * legacy;
        }
        if (!(h > 0) || h !== h) {
            h = def.h * legacy;
        }
        return {
            w: Math.max(0.03, Math.min(0.7, w)),
            h: Math.max(0.03, Math.min(0.7, h))
        };
    }

    function applyScenePinBoxSize(pinEl, pin) {
        if (!pinEl) {
            return null;
        }
        var size = normalizeScenePinSize(pin);
        // sizeW/sizeH are both fractions of frame WIDTH so box aspect = sizeW/sizeH
        // (independent of viewport / bake image aspect). Height via aspect-ratio.
        pinEl.style.width = (size.w * 100) + "%";
        pinEl.style.height = "auto";
        pinEl.style.aspectRatio = String(size.w) + " / " + String(size.h);
        pinEl.style.maxWidth = "none";
        pinEl.style.maxHeight = "none";
        pinEl.setAttribute("data-pin-size-w", String(size.w));
        pinEl.setAttribute("data-pin-size-h", String(size.h));
        if (pin) {
            pin.sizeW = size.w;
            pin.sizeH = size.h;
        }
        return size;
    }

    function paintSceneCellPins(overlays, pins, options) {
        var nextPins = pins || [];
        var structSig = nextPins.map(scenePinStructureKey).join(";");
        var prevSig = overlays.getAttribute("data-pins-struct") || "";
        var existing = overlays.children;
        // Same layout: refresh pin bodies in place (no empty flash / reflow of shells).
        if (prevSig === structSig && existing.length === nextPins.length) {
            nextPins.forEach(function (pin, index) {
                var pinEl = existing[index];
                pinEl.className = "scene-cell-pin scene-cell-pin--"
                    + (pin.viewStyle === "gauge" ? "gauge" : "icon");
                pinEl.style.left = (clamp01(pin.x) * 100) + "%";
                pinEl.style.top = (clamp01(pin.y) * 100) + "%";
                applyScenePinBoxSize(pinEl, pin);
                pinEl.innerHTML = "";
                if (typeof options.renderPin === "function") {
                    options.renderPin(pinEl, pin);
                } else {
                    pinEl.title = pin.kind || "pin";
                }
            });
            return;
        }
        var frag = document.createDocumentFragment();
        nextPins.forEach(function (pin) {
            var pinEl = document.createElement("div");
            pinEl.className = "scene-cell-pin scene-cell-pin--"
                + (pin.viewStyle === "gauge" ? "gauge" : "icon");
            pinEl.style.left = (clamp01(pin.x) * 100) + "%";
            pinEl.style.top = (clamp01(pin.y) * 100) + "%";
            applyScenePinBoxSize(pinEl, pin);
            if (typeof options.renderPin === "function") {
                options.renderPin(pinEl, pin);
            } else {
                pinEl.title = pin.kind || "pin";
            }
            frag.appendChild(pinEl);
        });
        overlays.innerHTML = "";
        overlays.appendChild(frag);
        overlays.setAttribute("data-pins-struct", structSig);
    }

    function mountSceneCell(host, sceneId, options) {
        options = options || {};
        host.classList.add("scene-cell");
        host.classList.remove("scene-cell--empty");
        if (!sceneId) {
            host.removeAttribute("data-scene-id");
            host.innerHTML = "";
            host.classList.add("scene-cell--empty");
            host.textContent = "No scene";
            return;
        }
        var sceneKey = String(sceneId);
        var reuse = host.getAttribute("data-scene-id") === sceneKey
            && host.querySelector("img.scene-cell-bake")
            && host.querySelector(".scene-cell-overlays");
        var frame;
        var img;
        var overlays;
        if (reuse) {
            frame = host.querySelector(".scene-cell-frame");
            img = host.querySelector("img.scene-cell-bake");
            overlays = host.querySelector(".scene-cell-overlays");
        } else {
            host.setAttribute("data-scene-id", sceneKey);
            host.innerHTML = "";
            frame = document.createElement("div");
            frame.className = "scene-cell-frame";
            img = document.createElement("img");
            img.className = "scene-cell-bake";
            img.alt = "Scene";
            overlays = document.createElement("div");
            overlays.className = "scene-cell-overlays";
            frame.appendChild(img);
            frame.appendChild(overlays);
            host.appendChild(frame);
        }
        fetch("/api/scenes/view?id=" + encodeURIComponent(sceneId), { cache: "no-store" })
            .then(function (response) {
                if (!response.ok) {
                    throw new Error("scene view failed");
                }
                return response.json();
            })
            .then(function (view) {
                // Stable bake URL — avoid cache-bust remount flicker on every console refresh.
                var bakeSrc = "/api/scenes/bake?id=" + encodeURIComponent(sceneId);
                if (img.getAttribute("src") !== bakeSrc) {
                    img.src = bakeSrc;
                }
                paintSceneCellPins(overlays, view.pins || [], options);
            })
            .catch(function () {
                host.removeAttribute("data-scene-id");
                host.classList.add("scene-cell--empty");
                host.textContent = "Scene unavailable";
            });
    }

    var sceneCellHostCache = {};

    function takeSceneCellHost(cacheKey, sceneId) {
        var key = String(cacheKey || "") + "|" + String(sceneId || "");
        var host = sceneCellHostCache[key];
        if (!host) {
            host = document.createElement("div");
            host.className = "scene-cell-host";
            sceneCellHostCache[key] = host;
        }
        return host;
    }

    function invalidateSceneCellHost(sceneId) {
        var suffix = "|" + String(sceneId || "");
        Object.keys(sceneCellHostCache).forEach(function (key) {
            if (key.length >= suffix.length && key.slice(-suffix.length) === suffix) {
                delete sceneCellHostCache[key];
            }
        });
    }

    global.SceneKit = {
        defaultModel: defaultModel,
        isSceneModelEmpty: isSceneModelEmpty,
        schemeModelFrom: schemeModelFrom,
        SceneDesigner: SceneDesigner,
        mountSceneCell: mountSceneCell,
        takeSceneCellHost: takeSceneCellHost,
        invalidateSceneCellHost: invalidateSceneCellHost,
        loadThreeJs: loadThreeJs,
        clamp01: clamp01,
        defaultScenePinSize: defaultScenePinSize,
        normalizeScenePinSize: normalizeScenePinSize,
        applyScenePinBoxSize: applyScenePinBoxSize
    };
})(window);
