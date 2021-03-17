global.choose = function(className) {
  const objects = [];
  var classToFind = eval('ObjC.classes.' + className);
  ObjC.choose(classToFind, {
    onMatch: function(cls) {
      objects.push(cls.toString());
    },
    onError: function() {
    },
    onComplete: function() {
    }
  });

  return objects;
}