/* Scene geometry designer + console cell viewer (custom Canvas, no three.js). */
(function (global) {
    "use strict";

    var DEFAULT_FLOOR_COLOR = "#6a8fad";
    var DEFAULT_WALL_THICKNESS = 0.01;
    var DEFAULT_WALL_HEIGHT = 0.15;

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

    function wallsAlongPolygon(polygon) {
        var walls = [];
        if (!polygon || polygon.length < 2) {
            return walls;
        }
        var index;
        for (index = 0; index < polygon.length; index++) {
            var a = polygon[index];
            var b = polygon[(index + 1) % polygon.length];
            walls.push({ x1: a.x, z1: a.z, x2: b.x, z2: b.z });
        }
        return walls;
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
        var polygon = defaultPolygon(sceneSize);
        return {
            y: 0,
            height: 0.3,
            color: DEFAULT_FLOOR_COLOR,
            polygon: polygon,
            walls: wallsAlongPolygon(polygon),
            windows: [],
            doors: []
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
        if (!Array.isArray(floor.walls)) {
            floor.walls = [];
        }
        if (!Array.isArray(floor.windows)) {
            floor.windows = [];
        }
        if (!Array.isArray(floor.doors)) {
            floor.doors = [];
        }
        if (!Array.isArray(floor.polygon) || floor.polygon.length < 3) {
            floor.polygon = polygonFromWalls(floor.walls) || defaultPolygon(sceneSize);
        }
        syncFloorOpenings(floor);
        return floor;
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

    function moveSharedWallCorner(floor, fromX, fromZ, toX, toZ) {
        if (!floor || !Array.isArray(floor.walls)) {
            return;
        }
        var epsilon = 1e-5;
        floor.walls.forEach(function (seg) {
            if (pointsNearEqual(seg.x1, seg.z1, fromX, fromZ, epsilon)) {
                seg.x1 = toX;
                seg.z1 = toZ;
            }
            if (pointsNearEqual(seg.x2, seg.z2, fromX, fromZ, epsilon)) {
                seg.x2 = toX;
                seg.z2 = toZ;
            }
        });
        syncFloorOpenings(floor);
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

    function syncFloorOpenings(floor) {
        if (!floor) {
            return;
        }
        ["windows", "doors"].forEach(function (listName) {
            (floor[listName] || []).forEach(function (opening) {
                syncOpeningGeometry(opening, floor.walls || []);
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

    function resolveShiftDragPoint(floor, handle, origin, raw, shiftHeld, sceneSize) {
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
            var seg = floor && floor.walls && floor.walls[handle.index];
            if (seg) {
                var fixed = handle.end === 1
                    ? { x: seg.x2, z: seg.z2 }
                    : { x: seg.x1, z: seg.z1 };
                return axisLockPoint(fixed, raw, true);
            }
            return raw;
        }
        // Corners (floor polygon) + openings: Shift locks horizontal/vertical motion.
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

    function projectOrbit(point, yaw, pitch, distance, width, height, pixelsPerUnit, perspective) {
        var cy = Math.cos(yaw);
        var sy = Math.sin(yaw);
        var cp = Math.cos(pitch);
        var sp = Math.sin(pitch);
        var x = point.x;
        var y = point.y;
        var z = point.z;
        // Orbit yaw around Y, then pitch so positive pitch looks down from above.
        var rx = x * cy + z * sy;
        var rz = -x * sy + z * cy;
        var ry = y * cp + rz * sp;
        var rz2 = -y * sp + rz * cp;
        var depth = distance + rz2;
        if (depth < 0.05) {
            depth = 0.05;
        }
        var base = pixelsPerUnit > 0 ? pixelsPerUnit : Math.min(width, height) * 0.42;
        // Perspective divides by depth; orthographic keeps a constant scale.
        var scale = perspective === false ? (base / Math.max(distance, 0.05)) : (base / depth);
        return {
            x: width * 0.5 + rx * scale,
            y: height * 0.58 - ry * scale,
            depth: depth
        };
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
            ["walls", "windows", "doors"].forEach(function (listName) {
                (floor[listName] || []).forEach(function (seg) {
                    includePoint(seg.x1, baseY, seg.z1);
                    includePoint(seg.x2, baseY, seg.z2);
                    includePoint(seg.x1, baseY + wallHeight, seg.z1);
                    includePoint(seg.x2, baseY + wallHeight, seg.z2);
                });
            });
        });
        if (!hasPoint) {
            var half = (model.sceneSize || 1) * 0.35;
            return { minX: -half, maxX: half, minY: 0, maxY: wallHeight, minZ: -half, maxZ: half };
        }
        return { minX: minX, maxX: maxX, minY: minY, maxY: maxY, minZ: minZ, maxZ: maxZ };
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

    function averageDepth(projected) {
        var sum = 0;
        var index;
        for (index = 0; index < projected.length; index++) {
            sum += projected[index].depth;
        }
        return sum / projected.length;
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
                    // Wall pieces that abut a window: noStroke so cut edges don't draw seams.
                    var abutOpts = cut.fillPanel ? { noStroke: true } : null;
                    if (t0 > cursor + 1e-4) {
                        solidRibbon(cursor, t0, y0, y1, wallColor, abutOpts);
                    }
                    if (cut.y0 > y0 + 1e-4) {
                        solidRibbon(t0, t1, y0, cut.y0, wallColor, abutOpts);
                    }
                    if (cut.y1 < y1 - 1e-4) {
                        solidRibbon(t0, t1, cut.y1, y1, wallColor, abutOpts);
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
                    } else {
                        var openingLen = (t1 - t0) * wallLength(center);
                        var jamb = Math.min(0.004, openingLen * 0.08);
                        if (jamb * 2.2 < openingLen) {
                            var dt = jamb / Math.max(wallLength(center), 1e-6);
                            solidRibbon(t0, t0 + dt, cut.y0, cut.y1, cut.color, { layer: 2 });
                            solidRibbon(t1 - dt, t1, cut.y0, cut.y1, cut.color, { layer: 2 });
                        }
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
                        lastCut && lastCut.fillPanel ? { noStroke: true } : null
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
            var abutOpts = cut.fillPanel ? { noStroke: true } : null;
            if (t0 > cursor + 1e-4) {
                solidSpan(cursor, t0, baseY, wallHeight, wallColor, abutOpts);
            }
            if (cut.y0 > baseY + 1e-4) {
                solidSpan(t0, t1, baseY, cut.y0 - baseY, wallColor, abutOpts);
            }
            if (cut.y1 < baseY + wallHeight - 1e-4) {
                solidSpan(t0, t1, cut.y1, (baseY + wallHeight) - cut.y1, wallColor, abutOpts);
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
            } else {
                var openingLen = (t1 - t0) * wallLength(wall);
                var jamb = Math.min(0.004, openingLen * 0.08);
                if (jamb * 2.2 < openingLen) {
                    var dt = jamb / wallLength(wall);
                    faces = faces.concat(wallSlab(
                        wall, t0, t0 + dt, cut.y0, cut.y1 - cut.y0, thickness, cut.color, 1,
                        { skipStartCap: true, skipEndCap: true, layer: 2 }
                    ));
                    faces = faces.concat(wallSlab(
                        wall, t1 - dt, t1, cut.y0, cut.y1 - cut.y0, thickness, cut.color, 1,
                        { skipStartCap: true, skipEndCap: true, layer: 2 }
                    ));
                }
            }
            cursor = Math.max(cursor, t1);
        });
        if (cursor < 1 - 1e-4) {
            var lastCut = cuts[cuts.length - 1];
            solidSpan(
                cursor,
                1,
                baseY,
                wallHeight,
                wallColor,
                lastCut && lastCut.fillPanel ? { noStroke: true } : null
            );
        }
        return faces;
    }

    function closedLoopFromWalls(walls) {
        if (!walls || walls.length < 3) {
            return null;
        }
        var epsilon = 1e-4;
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
            layer: 0
        };
    }

    function buildSceneFaces(model) {
        var faces = [];
        var wallHeight = model.wallHeight || DEFAULT_WALL_HEIGHT;
        var thickness = model.wallThickness || DEFAULT_WALL_THICKNESS;
        var wallColor = "#cfd6e0";
        (model.floors || []).forEach(function (floor) {
            // All floors share ground level (no stacked multi-storey offset).
            var baseY = 0;
            syncFloorOpenings(floor);
            var walls = floor.walls || [];
            var loop = closedLoopFromWalls(walls);
            var floorFace = makeFloorFace(floor.polygon, baseY, floor.color);
            if (loop) {
                floorFace = makeFloorFace(loop, baseY, floor.color) || floorFace;
            }
            if (floorFace) {
                faces.push(floorFace);
            }

            // Doors = void + jambs; windows = cyan panels cut into the wall (no overlapping z-fight).
            var openings = [];
            (floor.doors || []).forEach(function (seg) {
                openings.push({
                    seg: seg,
                    color: "#ffb74d",
                    y0: baseY,
                    y1: baseY + wallHeight * 0.92,
                    fillPanel: false
                });
            });
            (floor.windows || []).forEach(function (seg) {
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

            if (loop) {
                faces = faces.concat(closedLoopWallFaces(
                    loop, openings, baseY, wallHeight, thickness, wallColor
                ));
            } else {
                walls.forEach(function (seg, index) {
                    var join = {
                        connectedStart: wallConnectedAt(walls, index, 1),
                        connectedEnd: wallConnectedAt(walls, index, 2)
                    };
                    faces = faces.concat(wallFacesWithOpenings(
                        seg, openings, baseY, wallHeight, thickness, join
                    ));
                });
            }
        });
        return faces;
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
            // Screen-space cull (canvas Y grows downward).
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
        // Floors first, then walls, then openings — keeps walls opaque over the floor plate.
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

    function SceneDesigner(canvas) {
        this.canvas = canvas;
        this.ctx = canvas.getContext("2d");
        this.model = defaultModel();
        this.floorIndex = 0;
        this.tool = "wall";
        this.viewMode = false;
        this.yaw = 0.85;
        this.pitch = 0.62;
        this.viewZoom = 1;
        this.perspective = true;
        this.editZoom = 1;
        this.dragOrbit = false;
        this.dragHandle = null;
        this.dragCornerFrom = null;
        this.dragCornerOrigin = null;
        this.dragShiftHeld = false;
        this.lastDragClientX = 0;
        this.lastDragClientY = 0;
        this.lastX = 0;
        this.lastY = 0;
        this.onChange = null;
        this.selection = null;
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
            if (event.key === "Delete" || event.key === "Del") {
                var active = document.activeElement;
                var tag = active && active.tagName ? active.tagName.toLowerCase() : "";
                if (tag === "input" || tag === "textarea" || tag === "select"
                    || (active && active.isContentEditable)) {
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
                self.viewZoom = Math.max(0.35, Math.min(3.5, self.viewZoom * factor));
                self.model.viewZoom = self.viewZoom;
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
    };

    SceneDesigner.prototype.dispose = function () {
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
        this.onChange = null;
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
        if (!(this.model.wallThickness > 0)) {
            this.model.wallThickness = DEFAULT_WALL_THICKNESS;
        }
        if (!(this.model.wallHeight > 0)) {
            this.model.wallHeight = DEFAULT_WALL_HEIGHT;
        }
        var sceneSize = this.model.sceneSize || 1;
        this.model.floors = this.model.floors.map(function (floor) {
            return normalizeFloor(floor, sceneSize);
        });
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
        this.perspective = this.model.perspective !== false;
        this.model.perspective = this.perspective;
        this.editZoom = 1;
        this.floorIndex = 0;
        this.selection = null;
        this.dragHandle = null;
        this.dragCornerFrom = null;
        this.dragCornerOrigin = null;
        this.dragShiftHeld = false;
        this.draw();
    };

    SceneDesigner.prototype.getModel = function () {
        this.model.viewZoom = this.viewZoom;
        this.model.yaw = this.yaw;
        this.model.pitch = this.pitch;
        this.model.perspective = this.perspective !== false;
        return cloneJson(this.model);
    };

    SceneDesigner.prototype.setPerspective = function (enabled) {
        this.perspective = !!enabled;
        this.model.perspective = this.perspective;
        if (typeof this.onChange === "function") {
            this.onChange();
        }
        this.draw();
        return this.perspective;
    };

    SceneDesigner.prototype.togglePerspective = function () {
        return this.setPerspective(!this.perspective);
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
        var floor = this.currentFloor();
        var offset = ((floor.walls || []).length % 6) * (length * 0.2);
        var midZ = offset - length * 0.4;
        floor.walls.push({
            x1: -length * 0.5 + offset * 0.25,
            z1: midZ,
            x2: length * 0.5 + offset * 0.25,
            z2: midZ
        });
        this.selection = { list: "walls", index: floor.walls.length - 1 };
        this.notify();
        this.draw();
    };

    SceneDesigner.prototype.setViewMode = function (enabled) {
        this.viewMode = !!enabled;
        this.dragHandle = null;
        this.draw();
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
            polygon: defaultPolygon(this.model.sceneSize || 1),
            walls: [],
            windows: [],
            doors: []
        };
        this.model.floors.push(floor);
        this.floorIndex = this.model.floors.length - 1;
        this.selection = { list: "polygon", index: 0 };
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
        floor[listName].push(seg);
        this.selection = { list: listName, index: floor[listName].length - 1 };
        this.notify();
        return true;
    };

    SceneDesigner.prototype.removeCurrentFloor = function () {
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
                polygon: defaultPolygon(this.model.sceneSize || 1),
                walls: [],
                windows: [],
                doors: []
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
        if (this.selection.list === "polygon") {
            var floorPolygon = this.currentFloor();
            if (!floorPolygon || !floorPolygon.polygon) {
                return false;
            }
            // Floor plate is independent of walls — clearing it keeps walls/windows/doors.
            if (floorPolygon.polygon.length <= 3) {
                floorPolygon.polygon = [];
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
        var floor = this.currentFloor();
        var list = floor[this.selection.list];
        if (list && this.selection.index >= 0 && this.selection.index < list.length) {
            list.splice(this.selection.index, 1);
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

    SceneDesigner.prototype.worldFromClient = function (clientX, clientY) {
        var rect = this.canvas.getBoundingClientRect();
        if (rect.width <= 0 || rect.height <= 0) {
            return { x: 0, z: 0 };
        }
        var px = (clientX - rect.left) / rect.width;
        var py = (clientY - rect.top) / rect.height;
        var size = this.model.sceneSize || 1;
        var zoom = this.viewMode ? 1 : Math.max(0.35, this.editZoom || 1);
        return {
            x: ((px - 0.5) / zoom) * size,
            z: ((py - 0.5) / zoom) * size
        };
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
        var activeFloor = this.currentFloor();
        var activeIndex = this.floorIndex;
        // Prefer active-floor wall/window/door endpoints over floor corners.
        ["walls", "windows", "doors"].forEach(function (listName) {
            var list = activeFloor[listName] || [];
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
                        best = {
                            list: listName,
                            index: index,
                            end: handle.end,
                            floorIndex: activeIndex
                        };
                    }
                });
            });
        });
        if (best) {
            return best;
        }
        // Floor polygon corners for EVERY floor so inactive plates stay editable.
        (this.model.floors || []).forEach(function (floor, floorIndex) {
            (floor.polygon || []).forEach(function (corner, index) {
                var dx = corner.x - point.x;
                var dz = corner.z - point.z;
                var dist = Math.sqrt(dx * dx + dz * dz);
                if (dist < bestDist) {
                    bestDist = dist;
                    best = {
                        list: "polygon",
                        index: index,
                        end: 0,
                        floorIndex: floorIndex
                    };
                }
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
        if (!best) {
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
                    best = { list: "polygon", index: index };
                }
            }
        }
        if (best) {
            best.floorIndex = this.floorIndex;
        }
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
        // Wall indices after the split shift — rebind openings by proximity.
        ["windows", "doors"].forEach(function (listName) {
            (floor[listName] || []).forEach(function (opening) {
                delete opening.wallIndex;
                delete opening.t0;
                delete opening.t1;
            });
        });
        syncFloorOpenings(floor);
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
            this.dragHandle = handle;
            this.selection = {
                list: handle.list,
                index: handle.index,
                floorIndex: handle.floorIndex
            };
            var floor = this.floorForHandle(handle);
            var corner;
            if (handle.list === "polygon") {
                corner = floor.polygon[handle.index];
            } else {
                var handleSeg = floor[handle.list][handle.index];
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
        // Click an inactive floor plate to activate it (so its corners can be deleted).
        var floorHit = this.findFloorIndexAt(point);
        if (floorHit >= 0 && floorHit !== this.floorIndex) {
            this.floorIndex = floorHit;
            this.selection = null;
            this.draw();
        }
        // Empty canvas never creates geometry — only toolbar tools add objects.
        this.selectNear(point);
    };

    SceneDesigner.prototype.applyHandleDragAtClient = function (clientX, clientY, shiftHeld) {
        if (!this.dragHandle || this.viewMode) {
            return;
        }
        var floor = this.floorForHandle(this.dragHandle);
        if (!this.dragCornerFrom) {
            return;
        }
        var raw = this.worldFromClient(clientX, clientY);
        var locked = resolveShiftDragPoint(
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
        var list = floor[this.dragHandle.list];
        if (!list || !list[this.dragHandle.index]) {
            return;
        }
        if (this.dragHandle.list === "walls") {
            // Move only this wall's shared wall-corner group — never the floor polygon.
            moveSharedWallCorner(
                floor,
                this.dragCornerFrom.x,
                this.dragCornerFrom.z,
                point.x,
                point.z
            );
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
            var hostWall = (typeof seg.wallIndex === "number")
                ? floor.walls[seg.wallIndex]
                : null;
            if (hostWall) {
                var p1 = projectOnSeg(seg.x1, seg.z1, hostWall);
                var p2 = projectOnSeg(seg.x2, seg.z2, hostWall);
                seg.t0 = Math.min(p1.t, p2.t);
                seg.t1 = Math.max(p1.t, p2.t);
                syncOpeningGeometry(seg, floor.walls);
            } else {
                bindOpeningToNearestWall(seg, floor.walls || []);
                syncOpeningGeometry(seg, floor.walls || []);
            }
        }
        this.dragCornerFrom = { x: point.x, z: point.z };
        this.draw();
    };

    SceneDesigner.prototype.finishHandleDrag = function (event) {
        if (!this.dragHandle) {
            return;
        }
        var floor = this.floorForHandle(this.dragHandle);
        if (this.dragCornerFrom) {
            var shiftHeld = event ? !!event.shiftKey : this.dragShiftHeld;
            var raw = event
                ? this.worldFromClient(event.clientX, event.clientY)
                : { x: this.dragCornerFrom.x, z: this.dragCornerFrom.z };
            var locked = resolveShiftDragPoint(
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
                    floor,
                    this.dragCornerFrom.x,
                    this.dragCornerFrom.z,
                    locked.x,
                    locked.z
                );
            } else {
                var snapped = this.snapJoin(locked, this.dragHandle);
                if (this.dragHandle.list === "walls") {
                    moveSharedWallCorner(
                        floor,
                        this.dragCornerFrom.x,
                        this.dragCornerFrom.z,
                        snapped.x,
                        snapped.z
                    );
                } else {
                    var list = floor[this.dragHandle.list];
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
                            bindOpeningToNearestWall(seg, floor.walls || []);
                            syncOpeningGeometry(seg, floor.walls || []);
                        }
                    }
                }
            }
        }
        this.dragHandle = null;
        this.dragCornerFrom = null;
        this.dragCornerOrigin = null;
        this.dragShiftHeld = false;
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
        var thicknessPx = Math.max(4, (this.getWallThickness() / size) * Math.min(width, height));
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
        var editZoom = Math.max(0.35, this.editZoom || 1);
        function toPx(point) {
            return {
                x: width * 0.5 + (point.x / size) * width * editZoom,
                y: height * 0.5 + (point.z / size) * height * editZoom
            };
        }
        function drawFloorPlate(floor, active) {
            var polygon = floor.polygon || [];
            if (polygon.length < 3) {
                return;
            }
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
            ctx.fillStyle = hexToRgba(floor.color || DEFAULT_FLOOR_COLOR, active ? 0.38 : 0.2);
            ctx.fill();
            ctx.strokeStyle = active ? "#9fb6c9" : "#5a6a7c";
            ctx.lineWidth = 1.5;
            ctx.stroke();
        }
        this.model.floors.forEach(function (floor, index) {
            drawFloorPlate(floor, index === self.floorIndex);
        });
        function drawSegs(list, listName, color, widthPx, activeFloor) {
            (list || []).forEach(function (seg, index) {
                var selected = activeFloor
                    && self.selection
                    && self.selection.list === listName
                    && self.selection.index === index;
                var a = toPx({ x: seg.x1, z: seg.z1 });
                var b = toPx({ x: seg.x2, z: seg.z2 });
                ctx.strokeStyle = selected ? "#ce93d8" : color;
                ctx.lineWidth = selected ? widthPx + 2 : widthPx;
                ctx.globalAlpha = activeFloor ? 1 : 0.45;
                ctx.lineCap = "butt";
                ctx.beginPath();
                ctx.moveTo(a.x, a.y);
                ctx.lineTo(b.x, b.y);
                ctx.stroke();
                ctx.globalAlpha = 1;
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
        // Draw every floor's walls on the scheme; only the active floor is editable/highlighted.
        this.model.floors.forEach(function (floor, index) {
            syncFloorOpenings(floor);
            var active = index === self.floorIndex;
            var wallColor = active ? "#d7dee8" : "#7a8494";
            drawSegs(floor.walls, "walls", wallColor, thicknessPx, active);
            drawSegs(floor.windows, "windows", active ? "#4fc3f7" : "#2f6f86", Math.max(2, thicknessPx * 0.55), active);
            drawSegs(floor.doors, "doors", active ? "#ffb74d" : "#8a6430", Math.max(2, thicknessPx * 0.55), active);
        });
        // Polygon handles for all floors (inactive plates were undraggable / undeletable).
        this.model.floors.forEach(function (floor, index) {
            drawPolygonHandles(floor.polygon, index, index === self.floorIndex);
        });
        var floor = this.currentFloor();
        drawHandles(floor.walls, "walls", "#eceff1");
        drawHandles(floor.windows, "windows", "#4fc3f7");
        drawHandles(floor.doors, "doors", "#ffb74d");
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
        ctx.fillStyle = "#10131a";
        ctx.fillRect(0, 0, width, height);
        var gradient = ctx.createLinearGradient(0, height * 0.45, 0, height);
        gradient.addColorStop(0, "rgba(30,36,48,0)");
        gradient.addColorStop(1, "rgba(20,24,34,0.9)");
        ctx.fillStyle = gradient;
        ctx.fillRect(0, 0, width, height);
        this.model.perspective = this.perspective !== false;
        var camera = viewProjectionParams(this.model, width, height, this.yaw, this.pitch);
        paintFaces(
            ctx,
            buildSceneFaces(this.model),
            this.yaw,
            this.pitch,
            camera.distance,
            width,
            height,
            camera.pixelsPerUnit * (this.viewZoom || 1),
            { x: camera.centerX, y: camera.centerY, z: camera.centerZ },
            camera.perspective
        );
        ctx.fillStyle = "#9aa4b5";
        ctx.font = "12px sans-serif";
        ctx.fillText(
            "View scene · drag to orbit · wheel zoom"
                + (this.perspective !== false ? " · perspective" : " · orthographic"),
            10,
            18
        );
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

    SceneDesigner.prototype.bakeJpegBlob = function (quality) {
        // Same aspect as the live View canvas so framing matches what the operator sees.
        var srcW = Math.max(1, this.canvas.width || 640);
        var srcH = Math.max(1, this.canvas.height || 420);
        var scale = Math.max(1, Math.min(4, 1920 / Math.max(srcW, srcH)));
        var width = Math.max(1, Math.round(srcW * scale));
        var height = Math.max(1, Math.round(srcH * scale));
        var off = document.createElement("canvas");
        off.width = width;
        off.height = height;
        var ctx = off.getContext("2d");
        ctx.fillStyle = "#10131a";
        ctx.fillRect(0, 0, width, height);
        var gradient = ctx.createLinearGradient(0, height * 0.4, 0, height);
        gradient.addColorStop(0, "rgba(30,36,48,0)");
        gradient.addColorStop(1, "rgba(18,22,30,0.95)");
        ctx.fillStyle = gradient;
        ctx.fillRect(0, 0, width, height);
        // Match the live View scene camera exactly (yaw / pitch / zoom / perspective).
        var yaw = this.yaw;
        var pitch = this.pitch;
        this.model.perspective = this.perspective !== false;
        var camera = viewProjectionParams(this.model, width, height, yaw, pitch);
        paintFaces(
            ctx,
            buildSceneFaces(this.model),
            yaw,
            pitch,
            camera.distance,
            width,
            height,
            camera.pixelsPerUnit * (this.viewZoom || 1),
            { x: camera.centerX, y: camera.centerY, z: camera.centerZ },
            camera.perspective
        );
        // Crop empty margins on all sides, keep ~1 cm padding (≈38px at 96 dpi × bake scale).
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
            off,
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

    function paintSceneCellPins(overlays, pins, options) {
        overlays.innerHTML = "";
        (pins || []).forEach(function (pin) {
            var pinEl = document.createElement("div");
            pinEl.className = "scene-cell-pin scene-cell-pin--"
                + (pin.viewStyle === "gauge" ? "gauge" : "icon");
            pinEl.style.left = (clamp01(pin.x) * 100) + "%";
            pinEl.style.top = (clamp01(pin.y) * 100) + "%";
            if (typeof options.renderPin === "function") {
                options.renderPin(pinEl, pin);
            } else {
                pinEl.title = pin.kind || "pin";
            }
            overlays.appendChild(pinEl);
        });
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
        SceneDesigner: SceneDesigner,
        mountSceneCell: mountSceneCell,
        takeSceneCellHost: takeSceneCellHost,
        invalidateSceneCellHost: invalidateSceneCellHost,
        clamp01: clamp01
    };
})(window);
