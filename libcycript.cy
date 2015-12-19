let $cy_set = function(object, properties) {
    for (const name in properties)
        Object.defineProperty(object, name, {
            configurable: true,
            enumerable: false,
            writable: true,
            value: properties[name],
        });
};

$cy_set(Date.prototype, {
    toCYON: function() {
        return `new ${this.constructor.name}(${this.toUTCString().toCYON()})`;
    },
});

$cy_set(Error.prototype, {
    toCYON: function() {
        return `new ${this.constructor.name}(${this.message.toCYON()})`;
    },
});
