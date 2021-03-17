global.choose = function(className) {
  const objects = []
  var classToFind = eval('ObjC.classes.' + className);
  ObjC.choose(classToFind, {
    onMatch: function(a) {
      objects.push(a.toString());
    },
    onComplete: function() {
    }
  });
  return objects;;
};
